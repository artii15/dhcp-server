#ifndef ALLOCATED_ADDRESS_H
#define ALLOCATED_ADDRESS_H

#include <stdint.h>
#include <list>
#include "pool_descriptor.h"

struct AllocatedAddress {
	uint32_t ipAddress;
	uint32_t mask;
	const std::list<uint32_t>* dnsServers;
	const std::list<uint32_t>* routers;
	uint32_t leaseTime;
};

#endif
