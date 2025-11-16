#pragma once

#include <cstdint>

enum EMsgType
{
	EMSG_REQUEST_USERSTATS_RESPONSE = 0x80000333,
	EMSG_APPOWNERSHIPTICKET_RESPONSE = 0x8000035a
};


class CProtoBufMsgBase
{
public:
	char __pad_0x0[0x14];	//0x0
	uint32_t type;			//0x14
	char __pad_0x18[0x8];	//0x18
	void* body;				//0x20
	char __pad_0x24[0x8];	//0x24
}; //0x2C


class CMsgAppOwnershipTicketResponse
{
public:
	char __pad_0x0[0x10];	//0x0
	//Ticket gets accessed like this: pTicket = *(int *)(*(uint *)(local_48.pBody_likely + 0x10) & 0xfffffffe);
	void** ppTicket;			//0x10
	uint32_t appId;			//0x14
	uint32_t result;		//0x18
}; //0x1C ?
