#include "../inc/discover_handler.h"

DiscoverHandler::DiscoverHandler(TransactionsStorage& storage, Client& clientToHandle, AddressesAllocator& addrAllocator)
	: transactionsStorage(storage), client(clientToHandle), allocator(addrAllocator) {}

void DiscoverHandler::handle(struct DHCPMessage& message, Options& options) {
	if(!transactionsStorage.transactionExists(message.xid)) {
		AllocatedAddress& address = allocator.allocateAddressFor(client);

		transactionsStorage.createTransaction(message.xid, &address);
		sendOffer(message, address);
	}
}

void DiscoverHandler::sendOffer(DHCPMessage& request, AllocatedAddress& allocatedAddress) {
	
}
