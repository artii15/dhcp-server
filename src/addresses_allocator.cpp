#include "../inc/addresses_allocator.h"

using namespace std;

AddressesAllocator::AddressesAllocator(Config& configToUse):config(configToUse) {
	const list<PoolDescriptor>& poolsDescriptors = config.getPoolsDescriptors();

	for(list<PoolDescriptor>::const_iterator descriptorsIt = poolsDescriptors.begin(); descriptorsIt != poolsDescriptors.end(); descriptorsIt++) {
		const PoolDescriptor& poolDescriptor = *descriptorsIt;
		AddressesPool* pool = new AddressesPool(poolDescriptor);

		addressesPools[pool->getNetworkAddress()] = pool;
	}
}

AddressesAllocator::~AddressesAllocator() {
	for(unordered_map<uint32_t, AddressesPool*>::iterator poolsIt = addressesPools.begin(); poolsIt != addressesPools.end(); poolsIt++) {
		delete poolsIt->second;
	}
}

// TODO: Remove this method later
AllocatedAddress AddressesAllocator::allocate(const HardwareAddress& clientAddress, uint32_t giaddr, uint32_t preferedAddress) {
	//uint32_t clientNetwork = determineClientNetwork(giaddr);
	//AddressesPool* pool = addressesPools[clientNetwork];

	//AllocatedAddress allocatedAddress = pool->getNext();
	//allocatedByHardware[1][clientAddress] = allocatedAddress;

	//return allocatedAddress;
	return AllocatedAddress();
}


AllocatedAddress AddressesAllocator::reallocateOldAddress(const Client& client) {
	
	return AllocatedAddress();
}

AllocatedAddress AddressesAllocator::allocateNewAddress(const Client& client) {
	return AllocatedAddress();	
}

void AddressesAllocator::allocate(uint32_t networkAddress, HardwareAddress& hardwareAddress, uint32_t address) {
	const PoolDescriptor& poolDescriptor = addressesPools[networkAddress]->descriptor;

	AllocatedAddress allocatedAddress;
	allocatedAddress.ipAddress = address;
	allocatedAddress.mask = poolDescriptor.networkMask;
	allocatedAddress.dnsServers = poolDescriptor.dnsServers;
	allocatedAddress.routers = poolDescriptor.routers;
	allocatedAddress.leaseTime = poolDescriptor.leaseTime;
	
	allocatedByHardware[networkAddress][hardwareAddress] = allocatedAddress;
}

void AddressesAllocator::allocate(uint32_t networkAddress, ClientSpecialId& specialId, uint32_t address) {
	allocatedBySpecialId[networkAddress][specialId] = AllocatedAddress();
}

uint32_t AddressesAllocator::determineClientNetwork(uint32_t giaddr) {
	return !giaddr ? config.getNetworkAddress() : matchNetworkToAddress(giaddr);
}

uint32_t AddressesAllocator::matchNetworkToAddress(uint32_t address) {
	for(unordered_map<uint32_t, AddressesPool*>::iterator poolsIt = addressesPools.begin(); poolsIt != addressesPools.end(); poolsIt++) {
		uint32_t networkAddress = poolsIt->first;
		AddressesPool* pool = poolsIt->second;

		if(pool->mayContain(address)) {
			return networkAddress;
		}
	}
	throw runtime_error("Provided address does not belong to any known network!");
}

bool AddressesAllocator::hasClientAllocatedAddress(const Client& client) {
	if(client.identificationMethod == BASED_ON_HARDWARE) {
		return allocatedByHardware.find(client.networkAddress) != allocatedByHardware.end() 
				&& allocatedByHardware[client.networkAddress].find(client.hardwareAddress) != allocatedByHardware[client.networkAddress].end();
	}
	else {
		return allocatedBySpecialId.find(client.networkAddress) != allocatedBySpecialId.end() 
				&& allocatedBySpecialId[client.networkAddress].find(client.specialId) != allocatedBySpecialId[client.networkAddress].end();
	}
}
