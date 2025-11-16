#include "ticket.hpp"

#include "../config.hpp"
#include "../globals.hpp"

#include "../sdk/IClientUtils.hpp"
#include "../sdk/EResult.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>

uint32_t Ticket::oneTimeSteamIdSpoof = 0;
uint32_t Ticket::tempSteamIdSpoof = 0;
std::map<uint32_t, CAppTicket> Ticket::ticketMap = std::map<uint32_t, CAppTicket>();
std::map<uint32_t, CEncryptedAppTicket> Ticket::encryptedTicketMap = std::map<uint32_t, CEncryptedAppTicket>();

std::string Ticket::getTicketDir()
{
	std::stringstream ss;
	ss << g_config.getDir().c_str() << "/cache";

	const auto dir = ss.str();
	if (!std::filesystem::exists(dir.c_str()))
	{
		std::filesystem::create_directory(dir.c_str());
	}

	return ss.str();
}

std::string Ticket::getTicketPath(uint32_t appId)
{
	std::stringstream ss;
	ss << getTicketDir().c_str() << "/ticket_" << appId;

	return ss.str();
}

CAppTicket Ticket::getCachedTicket(uint32_t appId)
{
	if (ticketMap.contains(appId))
	{
		return ticketMap[appId];
	}

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
	
	ticketMap[appId] = ticket;

	return ticket;
}

bool Ticket::saveTicketToCache(uint32_t appId, void* ticketData, uint32_t ticketSize)
{
	CAppTicket ticket {};
	g_pLog->debug("Saving ticket for %u...\n", appId);

	memcpy(&ticket, ticketData, ticketSize);

	const auto path = getTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(reinterpret_cast<char*>(&ticket), sizeof(ticket));

	g_pLog->once("Saved ticket for %u\n", appId);

	ticketMap[appId] = ticket;
	
	return true;
}

void Ticket::recvAppOwnershipTicketResponse(CMsgAppOwnershipTicketResponse* resp)
{
	//Very weird way to store data, thanks GCC
	//TODO: Maybe dig in further to improve my classes
	uint32_t* pSize = (reinterpret_cast<uint32_t*>(*resp->ppTicket) - 3);

	switch (resp->result)
	{

		case ERESULT_OK:
			saveTicketToCache(resp->appId, *resp->ppTicket, *pSize);
			return;

		case ERESULT_ACCESS_DENIED:
			auto cached = getCachedTicket(resp->appId);
			if (!cached.steamId)
			{
				return;
			}

			const uint32_t size = cached.getSize();

			//cached.issueTime = std::chrono::duration_cast<std::chrono::seconds>
			//(
			//	std::chrono::system_clock::now().time_since_epoch()
			//).count();
			//TODO: Somehow resign ticket after updating issue time

			memcpy(*resp->ppTicket, &cached, 0x400);
			*pSize = size;
			resp->result = ERESULT_OK;
			break;
	}
}

void Ticket::getTicketOwnershipExtendedData(uint32_t appId)
{
	const CAppTicket cached = Ticket::getCachedTicket(appId);
	const uint32_t steamId = cached.steamId;
	if (!steamId)
	{
		return;
	}

	oneTimeSteamIdSpoof = steamId;
}

std::string Ticket::getEncryptedTicketPath(uint32_t appId)
{
	std::stringstream ss;
	ss << getTicketDir().c_str() << "/encryptedTicket_" << appId;

	return ss.str();
}

CEncryptedAppTicket Ticket::getCachedEncryptedTicket(uint32_t appId)
{
	if (encryptedTicketMap.contains(appId))
	{
		return encryptedTicketMap[appId];
	}

	CEncryptedAppTicket ticket {};

	const auto path = getEncryptedTicketPath(appId);
	if (!std::filesystem::exists(path.c_str()))
	{
		return ticket;
	}

	std::ifstream ifs(path, std::ios::in);

	g_pLog->debug("Reading encrypted ticket for %u\n", appId);

	ifs.read(reinterpret_cast<char*>(&ticket), sizeof ticket);
	//g_pLog->debug("Ticket: %u, %u, %u\n", ticket.getSteamId(), ticket.getAppId(), ticket.getSize());

	encryptedTicketMap[appId] = ticket;

	return ticket;
}

bool Ticket::saveEncryptedTicketToCache(uint32_t appId, uint32_t steamId, void* ticketData, uint32_t written)
{
	CEncryptedAppTicket ticket {};
	g_pLog->debug("Saving encrypted ticket for %u...\n", appId);

	ticket.steamId = steamId;
	ticket.size = written;
	memcpy(ticket.bytes, ticketData, written);

	const auto path = getEncryptedTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(reinterpret_cast<char*>(&ticket), sizeof(ticket));

	g_pLog->once("Saved encrypted ticket for %u\n", appId);

	encryptedTicketMap[appId] = ticket;
	
	return true;
}

bool Ticket::getEncryptedAppTicket(void* ticketData, uint32_t ticketSize, uint32_t* bytesWritten)
{
	if (!ticketData || !ticketSize || !bytesWritten)
	{
		//This shouldn't happen, but some games seem to do it for some reason (possible a pitfall idk)
		return false;
	}

	bool cached = false;

	if (*bytesWritten)
	{
		saveEncryptedTicketToCache(g_pClientUtils->getAppId(), g_currentSteamId, ticketData, *bytesWritten);
		cached = true;
	}

	if (g_config.blockEncryptedAppTickets)
	{
		memset(ticketData, 0, ticketSize);
		*bytesWritten = 0;
	}

	if (cached)
	{
		return false;
	}

	CEncryptedAppTicket ticket = Ticket::getCachedEncryptedTicket(g_pClientUtils->getAppId());
	//TODO: Add isValid helper function to ticket or similiar
	if (!ticket.size || !ticket.steamId)
	{
		return false;
	}

	if (ticket.size > ticketSize)
	{
		g_pLog->debug("Failed to use use cached AppTicket for %u (Allocated size < bytesWritten)!\n");
		return false;
	}

	//We're running on the assumption that tickets won't be bigger than 0x1000 bytes
	tempSteamIdSpoof = ticket.steamId;
	memcpy(ticketData, ticket.bytes, ticketSize);
	*bytesWritten = ticket.size;

	return true;
}

bool Ticket::getAPICallResult(ECallbackType type, void* pCallback)
{
	if (type != ECallbackType::RequestEncryptedAppOwnershipTicket)
	{
		return false;
	}

	uint32_t* pResult = reinterpret_cast<uint32_t*>(pCallback);

	if (*pResult == 1)
	{
		return false;
	}

	CEncryptedAppTicket ticket = getCachedEncryptedTicket(g_pClientUtils->getAppId());
	if (!ticket.size || !ticket.steamId)
	{
		return false;
	}

	*pResult = 1; //Success
	g_pLog->debug("Spoofed RequestEncryptedAppOwnershipTicket callback!\n");

	return true;
}

