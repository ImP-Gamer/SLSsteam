#include "ticket.hpp"

#include "../config.hpp"
#include "../globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>

uint32_t Ticket::steamIdSpoof = 0;

std::string Ticket::getTicketPath(uint32_t appId)
{
	std::stringstream ss;
	ss << g_config.getDir().c_str() << "/cache";

	const auto dir = ss.str();
	if (!std::filesystem::exists(dir.c_str()))
	{
		std::filesystem::create_directory(dir.c_str());
	}

	ss << "/ticketData_" << appId;

	return ss.str();
}

CAppTicket Ticket::getCachedTicket(uint32_t appId)
{
	CAppTicket ticket {};

	const auto path = getTicketPath(appId);
	if (!std::filesystem::exists(path.c_str()))
	{
		return ticket;
	}

	std::ifstream ifs(path, std::ios::in);

	g_pLog->debug("Reading ticket for %u\n", appId);

	ifs.read(reinterpret_cast<char*>(&ticket), sizeof ticket);
	//g_pLog->debug("Ticket: %u, %u, %u\n", ticket.getSteamId(), ticket.getAppId(), ticket.getSize());

	return ticket;
}

bool Ticket::saveTicketToCache(uint32_t appId, void* ticketData, uint32_t ticketSize, uint32_t* a4)
{
	CAppTicket ticket {};
	g_pLog->debug("Saving ticket for %u...\n", appId);

	//steamId is in ticket too, but whatever
	memcpy(ticket.bytes, ticketData, ticketSize);
	memcpy(ticket.extraData, a4, sizeof(ticket.extraData));

	const auto path = getTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(reinterpret_cast<char*>(&ticket), sizeof(ticket));

	g_pLog->once("Saved ticket for %u\n", appId);
	
	return true;
}

uint32_t Ticket::getTicketOwnershipExtendedData(uint32_t appId, void* ticket, uint32_t ticketSize, uint32_t* a4)
{
	if (ticketSize)
	{
		saveTicketToCache(appId, ticket, ticketSize, a4);
		return 0;
	}

	const CAppTicket cached = Ticket::getCachedTicket(appId);
	const uint32_t size = cached.getSize();
	if (!size)
	{
		return 0;
	}

	steamIdSpoof = cached.getSteamId();

	memcpy(ticket, cached.bytes, size);
	memcpy(a4, cached.extraData, sizeof(cached.extraData));

	return size;
}
