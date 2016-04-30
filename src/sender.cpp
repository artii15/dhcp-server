#include "../inc/sender.h"
#include "../inc/protocol.h"
#include "../inc/option.h"

#include <linux/if_ether.h>

#define IP_BROADCAST_ADDR 0xffffffff
#define BROADCAST_ADDR_LEN 6

Sender::Sender(libnet_t* lnetHandle) {
	this->lnetHandle = lnetHandle;
}

void Sender::send(DHCPMessage& response, const AllocatedAddress&, unsigned messageType) {
	uint32_t targetIpAddress = 0;
	uint8_t targetHardwareAddress[BROADCAST_ADDR_LEN] = {0};
	if(response.giaddr != 0) {
		targetIpAddress = response.giaddr;
	}
	else if(messageType == DHCPNAK) {
		targetIpAddress = IP_BROADCAST_ADDR;
		fillBroadcastAddress(targetHardwareAddress);
	}
	else if(response.giaddr == 0 && response.ciaddr != 0) {
		targetIpAddress = response.ciaddr;
	}
	else if(response.flags & BROADCAST_FLAG) {
		targetIpAddress = IP_BROADCAST_ADDR;
		fillBroadcastAddress(targetHardwareAddress);
	}
	else {
		targetIpAddress = response.yiaddr;
		memcpy(targetHardwareAddress, response.chaddr, BROADCAST_ADDR_LEN);
	}

	libnet_build_udp(Protocol::getServicePortByName("bootps", "udp"), Protocol::getServicePortByName("bootpc", "udp"), LIBNET_UDP_H + sizeof(response), 0, (uint8_t*)&response, sizeof(response), lnetHandle, 0);

	libnet_autobuild_ipv4(LIBNET_IPV4_H + LIBNET_UDP_H + sizeof(response), IPPROTO_UDP, targetIpAddress, lnetHandle);

	uint8_t zeroAddr[BROADCAST_ADDR_LEN] = {0};
	if(memcmp(zeroAddr, targetHardwareAddress, BROADCAST_ADDR_LEN) != 0) {
		libnet_autobuild_ethernet(targetHardwareAddress, ETH_P_IP, lnetHandle);
	}

	libnet_write(lnetHandle);
	libnet_clear_packet(lnetHandle);
}

void Sender::fillBroadcastAddress(uint8_t* buffer) {
	uint8_t broadcastAddr[BROADCAST_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	memcpy(buffer, broadcastAddr, BROADCAST_ADDR_LEN);
}