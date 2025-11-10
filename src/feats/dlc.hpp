#pragma once

#include <cstddef>
#include <cstdint>


namespace DLC
{
	bool shouldUnlockDlc(uint32_t appId);

	bool isDlcEnabled(uint32_t appId);
	bool isSubscribed(uint32_t appId);
	bool isAppDlcInstalled(uint32_t appId);
	bool userSubscribedInTicket(uint32_t appId);

	uint32_t getDlcCount(uint32_t appId);
	bool getDlcDataByIndex(uint32_t appId, int index, uint32_t* dlcId, bool* available, char* dlcName, size_t& dlcNameLen);
}
