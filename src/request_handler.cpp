#include "../inc/request_handler.h"
#include "../inc/packer.h"

RequestHandler::RequestHandler(TransactionsStorage& storage, Client& clientToHandle, AddressesAllocator& addrAllocator, Server& serv)
	: transactionsStorage(storage), client(clientToHandle), allocator(addrAllocator), server(serv) {}


void RequestHandler::handle(struct DHCPMessage& request, Options& options) {
	ClientState clientState = determineClientState(request, options);

	switch(clientState) {
		case SELECTING:
			handleSelectingState(request, options);
			break;
		case INIT_REBOOT:
			handleInitRebootState(request, options);
			break;
		case RENEWING:
			break;
		case REBINDING:
			break;
		case UNKNOWN:
			break;
	}
	transactionsStorage.removeTransaction(request.xid);
}

ClientState RequestHandler::determineClientState(struct DHCPMessage& request, Options& options) {
	if(options.exists(SERVER_IDENTIFIER) && request.ciaddr == 0 && request.yiaddr != 0) {
		return SELECTING;
	}
	else if((!options.exists(SERVER_IDENTIFIER) || (uint32_t)*options.get(SERVER_IDENTIFIER).value == 0)
			&& request.yiaddr != 0 && request.ciaddr == 0) {
		return INIT_REBOOT;
	}
	else if((!options.exists(SERVER_IDENTIFIER) || (uint32_t)*options.get(SERVER_IDENTIFIER).value) 
			&& request.yiaddr == 0 && request.ciaddr != 0) {
		return RENEWING;
	}
	else if((!options.exists(SERVER_IDENTIFIER) || (uint32_t)*options.get(SERVER_IDENTIFIER).value) 
			&& request.ciaddr != 0) {
		return REBINDING;
	}
	else {
		return UNKNOWN;
	}
}

void RequestHandler::handleSelectingState(struct DHCPMessage& request, Options& options) {
	if(*options.get(SERVER_IDENTIFIER).value != server.serverIp) {
		allocator.freeClientAddress(client);
	}
	else if(transactionsStorage.transactionExists(request.xid)) {
		const AllocatedAddress& allocatedAddress = *transactionsStorage.getTransaction(request.xid).allocatedAddress;
		if(isRequestedAddressValid(request, options, allocatedAddress)) {
			respond(request, allocatedAddress, DHCPACK);
		}
		else {
			respond(request, allocatedAddress, DHCPNAK);
		}
	}
}

bool RequestHandler::isRequestedAddressValid(DHCPMessage& request, Options& options, const AllocatedAddress& allocatedAddress) {
	return options.exists(REQUESTED_IP_ADDRESS) 
		&& (uint32_t)*options.get(REQUESTED_IP_ADDRESS).value == allocatedAddress.ipAddress;
}

void RequestHandler::handleInitRebootState(struct DHCPMessage& request, Options& options) {
	if(allocator.hasClientAllocatedAddress(client)) {
		const AllocatedAddress& allocatedAddress = allocator.getAllocatedAddress(client);
		if(allocatedAddress.ipAddress == (uint32_t)*options.get(REQUESTED_IP_ADDRESS).value) {
			respond(request, allocatedAddress, DHCPACK);
		}
		else {
			AllocatedAddress invalidAddress = allocatedAddress;
			invalidAddress.ipAddress = (uint32_t)*options.get(REQUESTED_IP_ADDRESS).value;
			respond(request, invalidAddress, DHCPNAK);
		}
	}
}

void RequestHandler::respond(DHCPMessage& request, const AllocatedAddress& allocatedAddress, uint8_t messageType) {
	DHCPMessage response;
	memset(&response, 0, sizeof(response));

	response.op = BOOTREPLY;
	response.htype = request.htype;
	response.hlen = request.hlen;
	response.xid = request.xid;
	response.yiaddr = allocatedAddress.ipAddress;
	response.flags = request.flags;
	response.giaddr = request.giaddr;
	memcpy(response.chaddr, request.chaddr, MAX_HADDR_SIZE);
	response.magicCookie = request.magicCookie;


	Packer packer(response.options);
	packer.pack(IP_ADDRESS_LEASE_TIME, allocatedAddress.leaseTime)
		.pack(DHCP_MESSAGE_TYPE, messageType)
		.pack(SERVER_IDENTIFIER, server.serverIp)
		.pack(SUBNET_MASK, allocatedAddress.mask)
		.pack(ROUTERS, allocatedAddress.routers)
		.pack(DNS_OPTION, allocatedAddress.dnsServers)
		.pack(END_OPTION);
	
	server.sender->send(response, allocatedAddress, DHCPACK);
}
