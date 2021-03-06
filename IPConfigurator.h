#ifndef __IPCONFIGURATOR__
#define __IPCONFIGURATOR__

#include <stdint.h>

typedef struct {
	uint8_t currentQuarter;
	uint32_t ipAddress;
} s_IpAddressConfigurator;

s_IpAddressConfigurator IPConfigurator_create();
char IPConfigurator_addChar(s_IpAddressConfigurator *configurator, const char c);
void IPConfigurator_removeChar(s_IpAddressConfigurator *configurator);
void IPConfigurator_toString(s_IpAddressConfigurator *configurator, char *ip, char full);

#endif
