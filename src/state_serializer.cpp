#include "../inc/state_serializer.h"
#include <stdio.h>

using namespace std;

StateSerializer::StateSerializer(const char* filePath) {
	remove(filePath);
	file = fopen(filePath, "wb");
}

StateSerializer::~StateSerializer() {
	fclose(file);
}

void StateSerializer::serialize(const AllocatedAddress& address) {
	fwrite(&address.ipAddress, sizeof(address.ipAddress), 1, file);
	fwrite(&address.mask, sizeof(address.mask), 1, file);
	fwrite(&address.leaseTime, sizeof(address.leaseTime), 1, file);
	fwrite(&address.allocationTime, sizeof(address.allocationTime), 1, file);
	fwrite(&address.allocationTime, sizeof(address.allocationTime), 1, file);
	
	serialize(*address.dnsServers);
	serialize(*address.routers);
}

void StateSerializer::serialize(const list<uint32_t>& uint32List) {
	serialize(uint32List.size());

	for(list<uint32_t>::const_iterator it = uint32List.begin(); it != uint32List.end(); it++) {
		fwrite(&*it, sizeof(uint32_t), 1, file);
	}
}

void StateSerializer::serialize(const unsigned value) {
	fwrite(&value, sizeof(value), 1, file);
}

void StateSerializer::serialize(const HardwareAddress& hardwareAddress) {

}
