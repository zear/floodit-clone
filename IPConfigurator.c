#include "IPConfigurator.h"
#include <string.h>
#include <stdio.h>

s_IpAddressConfigurator IPConfigurator_create() {
	s_IpAddressConfigurator configurator;
	configurator.selectedDigit = 0;
	configurator.ipAddress = 0;
	configurator.currentQuarter = 4;
	return configurator;
}

char IPConfigurator_addChar(s_IpAddressConfigurator *configurator, const uint8_t c) {
	uint8_t quarterValue, newValue, shift;
	if (configurator->currentQuarter <= 0) {
		return 0;
	}

	if (c == '.') {
		configurator->currentQuarter--;
		return 1;
	}

	shift = (configurator->currentQuarter - 1) * 8;
	quarterValue = 255 & (configurator->ipAddress >> shift);
	newValue = quarterValue * 10 + c;
	if (newValue < quarterValue) {
		configurator->currentQuarter--;
		return IPConfigurator_addChar(configurator, c);
	}
	configurator->ipAddress &= ~(255 << shift);
	configurator->ipAddress |= newValue << shift;

	if (newValue > 25 || (c == 0 && !newValue)) {
		configurator->currentQuarter--;
	}

	return 1;
}

void IPConfigurator_toString(s_IpAddressConfigurator *configurator, char *ip, char full) {
	if (full) {
		snprintf(
			ip, 16,
			"%d.%d.%d.%d",
			255 & (configurator->ipAddress >> 24),
			255 & (configurator->ipAddress >> 16),
			255 & (configurator->ipAddress >> 8),
			255 & configurator->ipAddress
		);
		return;
	}

	int quarter = 4;
	uint8_t shift, value;
	char buff[4];

	ip[0] = '\0';
	while (quarter && quarter >= configurator->currentQuarter) {
		shift = (quarter - 1) * 8;
		value = 255 & (configurator->ipAddress >> shift);
		sprintf(buff, "%d", value);
		if (value || quarter > configurator->currentQuarter) {
			strncat(ip, buff, 3);
		}
		if (quarter > 1 && quarter > configurator->currentQuarter) {
			strncat(ip, ".", 1);
		}
		--quarter;
	}

	if (configurator->currentQuarter > 0) {
		strcat(ip, "_");
	}
}
