#include <WinSock2.h>
#include <stdio.h>

#include "lib\Library.h"
#include "MemoryPool.h"
#include "NPacket.h"
#include "StreamQueue.h"
#include "Config.h"
#include "LanServer.h"
#include "LanServerTest.h"

CLanServerTest::CLanServerTest()
	: CLanServer(){}

CLanServerTest::~CLanServerTest(){}

void CLanServerTest::OnClientJoin(SESSION_INFO *pSessionInfo, __int64 ClientID)		// Accept 후 접속처리 완료 후 호출.
{
	///////////////////////////////////////////////////////////////
	// Login Packet Send
	///////////////////////////////////////////////////////////////
	/*
	PRO_BEGIN(L"PacketAlloc");
	CNPacket *pLoginPacket = CNPacket::Alloc();
	PRO_END(L"PacketAlloc");

	*pLoginPacket << (short)8;
	*pLoginPacket << 0x7fffffffffffffff;

	PRO_BEGIN(L"SendPacket");
	SendPacket(ClientID, pLoginPacket);
	PRO_END(L"SendPacket");

	pLoginPacket->Free();
	*/
}

void CLanServerTest::OnClientLeave(__int64 ClientID)   					// Disconnect 후 호출
{

}

bool CLanServerTest::OnConnectionRequest(WCHAR *ClientIP, int Port)		// accept 직후
{
	return true;
}

void CLanServerTest::OnRecv(__int64 ClientID, CNPacket *pPacket)			// 패킷 수신 완료 후
{
	PRO_BEGIN(L"PacketAlloc");
	//CNPacket *pSendPacket = CNPacket::Alloc();
	CNPacket *pSendPacket = new CNPacket();

	PRO_END(L"PacketAlloc");

	short header;
	__int64 iValue;

	// Packet Process
	*pPacket >> header;
	*pPacket >> iValue;

	*pSendPacket << header;
	*pSendPacket << iValue;
	//////////////////////

	pSendPacket->iUsedSession = ClientID;
	pSendPacket->bUse = true;
	pSendPacket->iStatus = 2;
	pSendPacket->iAddress = (__int64)pPacket;

	PRO_BEGIN(L"SendPacket");
	SendPacket(ClientID, pSendPacket);
	PRO_END(L"SendPacket");

	//pSendPacket->Free();
	//delete pSendPacket;

	InterlockedIncrement64((LONG64 *)&_SendPacketCounter);
}

void CLanServerTest::OnSend(__int64 ClientID, int sendsize)				// 패킷 송신 완료 후
{

}

void CLanServerTest::OnWorkerThreadBegin()								// 워커스레드 GQCS 바로 하단에서 호출
{

}

void CLanServerTest::OnWorkerThreadEnd()								// 워커스레드 1루프 종료 후
{

}

void CLanServerTest::OnError(int errorCode, WCHAR *errorString)
{
	wprintf(L"ErrorCode : %d, ErrorMsg : %s\n", errorCode, errorString);
}