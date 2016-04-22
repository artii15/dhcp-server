#include "../inc/server.h"
#include "../inc/dhcp_message.h"

#include <netdb.h>
#include <stdio.h>

#define MAX_FILTER_SIZE 64

Server::Server(char* interfaceName) {
	pcapHandle = pcap_create(interfaceName, pcapErrbuf);
	if(pcapHandle == NULL) {
		throw pcapErrbuf;
	}
	pcap_set_snaplen(pcapHandle, 65535);
	if(pcap_activate(pcapHandle) != 0) {
		throw pcapErrbuf;
	}

	if(pcap_lookupnet(interfaceName, &serverIpAddr, &serverIpMask, pcapErrbuf) != 0) {
		throw pcapErrbuf;
	}

	setPacketsFilter();
}

void Server::setPacketsFilter() {
	struct bpf_program fp;

	struct servent* bootpServerService = getservbyname("bootps", "udp");
	struct servent* bootpClientService = getservbyname("bootpc", "udp");

	if(bootpServerService == 0 || bootpClientService == 0) {
		throw "Could determine bootp service port for udp";
	}

	char filter[MAX_FILTER_SIZE] = {0};
	snprintf(filter, MAX_FILTER_SIZE - 1, "udp dst port %u and udp src port %u", ntohs(bootpServerService->s_port), ntohs(bootpClientService->s_port));
	if(pcap_compile(pcapHandle, &fp, filter, 0, serverIpMask) != 0) {
		throw pcapErrbuf;
	}
	if(pcap_setfilter(pcapHandle, &fp) < 0) {
		throw pcapErrbuf;
	}
}

Server::~Server() {
	pcap_close(pcapHandle); 
}

void Server::listen() {
	pcap_loop(pcapHandle, 0, &Server::dispatch, (u_char*)this);
}

void Server::dispatch(u_char *server, const struct pcap_pkthdr *header, const u_char *rawMessage) {
	printf("[%dB of %dB]\n", header->caplen, header->len);
}
