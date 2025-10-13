#pragma once

#include "../sdk/CAppTicket.hpp"

#include <cstdint>
#include <string>


namespace Ticket
{
	extern uint32_t steamIdSpoof;

	//TODO: Fill with error checks
	std::string getTicketPath(uint32_t appId);
	CAppTicket getCachedTicket(uint32_t appId);
	bool saveTicketToCache(uint32_t appId, void* ticketData, uint32_t ticketSize, uint32_t* a4);

	uint32_t getTicketOwnershipExtendedData(uint32_t appId, void* ticket, uint32_t ticketSize, uint32_t* a4);//, uint32_t* a5, uint32_t* a6, uint32_t* a7);
}
