#include "../inc/server.h"
#include "../inc/dhcp_message.h"
#include "../inc/protocol.h"
#include "../inc/client.h"
#include "../inc/discover_handler.h"

#include <sys/ioctl.h>
#include <stdio.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdexcept>

#define MAX_FILTER_SIZE 64

using namespace std;

Server::Server(Config &configuration, AddressesAllocator& allocator, TransactionsStorage& storage)
 	  : config(configuration), addressesAllocator(allocator), transactionsStorage(storage) {

	networkResolver = new NetworkResolver(config);
	const char* interfaceName = config.getInterface();

	serverIp = determineDeviceIp(interfaceName);

	pcapHandle = pcap_create(interfaceName, pcapErrbuf);
	if(pcapHandle == NULL) {
		throw runtime_error(pcapErrbuf);
	}
	pcap_set_snaplen(pcapHandle, 65535);
	if(pcap_activate(pcapHandle) != 0) {
		throw runtime_error(pcapErrbuf);
	}

	lnetHandle = libnet_init(LIBNET_LINK, interfaceName, lnetErrbuf);

	setPacketsFilter();
}

uint32_t Server::determineDeviceIp(const char* interfaceName) {
	int fd = socket(AF_INET, SOCK_DGRAM, 0);

	struct ifreq ifr;
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, interfaceName, IFNAMSIZ-1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);

	return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
}

void Server::setPacketsFilter() {
	struct bpf_program fp;

	uint16_t bootpServerPort = Protocol::getServicePortByName("bootps", "udp");
	uint16_t bootpClientPort = Protocol::getServicePortByName("bootpc", "udp");

	char filter[MAX_FILTER_SIZE] = {0};
	snprintf(filter, MAX_FILTER_SIZE - 1, "ether proto 0x%04x and udp dst port %u and udp src port %u", ETH_P_IP, bootpServerPort, bootpClientPort);
	if(pcap_compile(pcapHandle, &fp, filter, 0, config.getNetworkMask()) != 0) {
		throw pcapErrbuf;
	}
	if(pcap_setfilter(pcapHandle, &fp) < 0) {
		throw pcapErrbuf;
	}
}

Server::~Server() {
	pcap_close(pcapHandle); 
	libnet_destroy(lnetHandle);
	delete networkResolver;
}

void Server::listen() {
	pcap_loop(pcapHandle, 0, &Server::dispatch, (u_char*)this);
}

void Server::dispatch(u_char *server, const struct pcap_pkthdr *header, const u_char *rawMessage) {
	unsigned int dhcpMsgStartPos = sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr);
	struct DHCPMessage* dhcpMsg = (struct DHCPMessage*)(rawMessage + dhcpMsgStartPos);
	Options options(dhcpMsg->options, header->caplen - dhcpMsgStartPos);

	Client client;
	memset(&client, 0, sizeof(client));

	client.hardwareAddress.addressType = dhcpMsg->htype;
	memcpy(client.hardwareAddress.hardwareAddress, dhcpMsg->chaddr, MAX_HADDR_SIZE);

	if(options.exists(CLIENT_IDENTIFIER)) {
		Option& clientIdOption = options.get(CLIENT_IDENTIFIER);
		client.specialId.type = *clientIdOption.value;
		memcpy(client.specialId.value, clientIdOption.value + 1, (CLIENT_SPECIAL_ID_MAX_LEN < clientIdOption.length) ? CLIENT_SPECIAL_ID_MAX_LEN : clientIdOption.length);

		client.identificationMethod = BASED_ON_SPECIAL_ID;
	}
	else {
		client.identificationMethod = BASED_ON_HARDWARE;
	}
	client.networkAddress = ((Server*)server)->networkResolver->determineNetworkAddress(dhcpMsg->giaddr);

	uint8_t operationType = *options.get(DHCP_MESSAGE_TYPE).value;
	Server& srv = *((Server*)server);
	switch(operationType) {
		case(DHCPDISCOVER): {
			DiscoverHandler(srv.transactionsStorage, client, srv.addressesAllocator, srv).handle(*dhcpMsg, options);
			break;	
		}
		case(DHCPREQUEST): {
			((Server*)server)->handleRequest(dhcpMsg, &options);
			break;	
		}
	}
}

uint8_t* Server::packIpAddressLeaseTime(uint8_t* dst, uint32_t leaseTime) {
	*(dst++) = IP_ADDRESS_LEASE_TIME;
	*(dst++) = sizeof(leaseTime);

	uint32_t normalizedLeaseTime = htonl(leaseTime);
	memcpy(dst, &normalizedLeaseTime, sizeof(normalizedLeaseTime));

	return dst + sizeof(normalizedLeaseTime);
}

uint8_t* Server::packMessageType(uint8_t* dst, uint8_t messageType) {
	*(dst++) = DHCP_MESSAGE_TYPE;
	*(dst++) = sizeof(messageType);
	*(dst++) = messageType;

	return dst;
}

uint8_t* Server::packServerIdentifier(uint8_t* dst) {
	*(dst++) = SERVER_IDENTIFIER;
	*(dst++) = sizeof(serverIp);
	memcpy(dst, &serverIp, sizeof(serverIp));

	return dst + sizeof(serverIp);
}

uint8_t* Server::packNetworkMask(uint8_t* dst, uint32_t mask) {
	*(dst++) = SUBNET_MASK;
	*(dst++) = sizeof(mask);

	uint32_t normalizedMask = htonl(mask);
	memcpy(dst, &normalizedMask, sizeof(mask));

	return dst + sizeof(mask);
}

uint8_t* Server::packRouters(uint8_t* dst, const list<uint32_t>* routers) {
	if(!routers->empty()) {
		*(dst++) = ROUTERS;

		uint32_t routersSizeInBytes = routers->size() * sizeof(uint32_t);
		*(dst++) = routersSizeInBytes;

		for(list<uint32_t>::const_iterator routersIt = routers->begin(); routersIt != routers->end(); routersIt++) {
			uint32_t router = htonl(*routersIt);
			memcpy(dst, &router, sizeof(router));
			dst += sizeof(router);
		}
	}

	return dst;
}

uint8_t* Server::packDnsServers(uint8_t* dst, const list<uint32_t>* servers) {
	if(!servers->empty()) {
		*(dst++) = DNS_OPTION;

		uint32_t serversSizeInBytes = servers->size() * sizeof(uint32_t);
		*(dst++) = serversSizeInBytes;

		for(list<uint32_t>::const_iterator serversIt = servers->begin(); serversIt != servers->end(); serversIt++) {
			uint32_t server = htonl(*serversIt);
			memcpy(dst, &server, sizeof(server));
			dst += sizeof(server);
		}
	}

	return dst;
}

void Server::handleRequest(struct DHCPMessage* dhcpMsg, Options* options) {
	if(transactionsStorage.transactionExists(dhcpMsg->xid)) {
		sendAck(dhcpMsg, *transactionsStorage.getTransaction(dhcpMsg->xid).allocatedAddress);
	}
}

void Server::sendAck(DHCPMessage* dhcpMsg, const AllocatedAddress &allocatedAddress) {
	DHCPMessage ack;
	memset(&ack, 0, sizeof(ack));

	ack.op = BOOTREPLY;
	ack.htype = dhcpMsg->htype;
	ack.hlen = dhcpMsg->hlen;
	ack.xid = dhcpMsg->xid;
	ack.yiaddr = htonl(allocatedAddress.ipAddress);
	ack.flags = dhcpMsg->flags;
	ack.giaddr = dhcpMsg->giaddr;
	memcpy(ack.chaddr, dhcpMsg->chaddr, MAX_HADDR_SIZE);
	ack.magicCookie = dhcpMsg->magicCookie;

	uint8_t* optionsPtr = packIpAddressLeaseTime(ack.options, allocatedAddress.leaseTime);
	optionsPtr = packMessageType(optionsPtr, DHCPACK);
	optionsPtr = packServerIdentifier(optionsPtr);
	optionsPtr = packNetworkMask(optionsPtr, allocatedAddress.mask);
	optionsPtr = packRouters(optionsPtr, allocatedAddress.routers);
	optionsPtr = packDnsServers(optionsPtr, allocatedAddress.dnsServers);
	*(optionsPtr++) = END_OPTION;

	libnet_build_udp(Protocol::getServicePortByName("bootps", "udp"), Protocol::getServicePortByName("bootpc", "udp"), LIBNET_UDP_H + sizeof(ack), 0, (uint8_t*)&ack, sizeof(ack), lnetHandle, 0);

	bool performBroadcast = (dhcpMsg->flags & BROADCAST_FLAG);
	libnet_autobuild_ipv4(LIBNET_IPV4_H + LIBNET_UDP_H + sizeof(ack), IPPROTO_UDP, performBroadcast ? 0xffffffffffffffff : ack.yiaddr, lnetHandle);

	uint8_t dstEthAddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	if(!performBroadcast) {
		memcpy(dstEthAddr, dhcpMsg->chaddr, 6);
	}
	libnet_autobuild_ethernet(dstEthAddr, ETH_P_IP, lnetHandle);

	libnet_write(lnetHandle);
	libnet_clear_packet(lnetHandle);
}
