#include "CAppTicket.hpp"

#include "../log.hpp"


uint32_t CAppTicket::getAppId() const
{
	return *reinterpret_cast<uint32_t const*>(bytes + extraData[0]);
}

uint32_t CAppTicket::getSteamId() const
{
	return *reinterpret_cast<uint32_t const*>(bytes + extraData[1]);
}

uint32_t CAppTicket::getSize() const
{
	//Ticket has random null bytes, so we have to do it like this (or cache the size)
	unsigned int last = 0;
	for(unsigned int i = 0; i < sizeof(bytes) / sizeof(uint8_t); i++)
	{
		if (bytes[i] != 0)
		{
			//Steam starts counting at 1
			last = i + 1;
		}
	}

	return last;
}
