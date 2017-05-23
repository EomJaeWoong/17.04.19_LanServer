#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mstcpip.h>
#include <process.h>
#include <stdio.h>

#pragma comment (lib, "Winmm.lib")
#pragma comment (lib, "Ws2_32.lib")

#include "lib\Library.h"
#include "MemoryPool.h"
#include "NPacket.h"
#include "Config.h"
#include "StreamQueue.h"
#include "LanServer.h"

CLanServer::CLanServer()
{
	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
		Session[iCnt]._bUsed = false;

	_bShutdown = false;
	_iSessionID = 0;
}

CLanServer::~CLanServer()
{

}

bool CLanServer::Start(WCHAR *wOpenIP, int iPort, int iWorkerThdNum, bool bNagle, int iMaxConnection)
{
	int retval;
	DWORD dwThreadID;

	_bShutdown = false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ���� �ʱ�ȭ
	//////////////////////////////////////////////////////////////////////////////////////////////////
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return FALSE;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Completion Port ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIOCP == NULL)
		return FALSE;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// socket ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	listen_sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listen_sock == INVALID_SOCKET)
		return FALSE;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//bind
	//////////////////////////////////////////////////////////////////////////////////////////////////
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(iPort);
	InetPton(AF_INET, wOpenIP, &serverAddr.sin_addr);
	retval = bind(listen_sock, (SOCKADDR *)&serverAddr, sizeof(SOCKADDR_IN));
	if (retval == SOCKET_ERROR)
		return FALSE;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//listen
	//////////////////////////////////////////////////////////////////////////////////////////////////
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		return FALSE;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// nagle �ɼ�
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_bNagle = bNagle;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Thread ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	hAcceptThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		AcceptThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	_iWorkerThdNum = iWorkerThdNum;

	for (int iCnt = 0; iCnt < iWorkerThdNum; iCnt++)
	{
		hWorkerThread[iCnt] = (HANDLE)_beginthreadex(
			NULL,
			0,
			WorkerThread,
			this,
			0,
			(unsigned int *)&dwThreadID
			);
	}

	hMonitorThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		MonitorThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	return TRUE;
}

void CLanServer::Stop()
{
	int retval;

	_bShutdown = true;

	HANDLE *hThread = new HANDLE[1 + _iWorkerThdNum];
	hThread[0] = hAcceptThread;
	for (int iCnt = 0; iCnt < _iWorkerThdNum; iCnt++)
		hThread[iCnt + 1] = hWorkerThread[iCnt];

	retval = WaitForMultipleObjects(1 + _iWorkerThdNum, hThread, TRUE, INFINITE);
	if (retval != WAIT_OBJECT_0)
		wprintf(L"Stop Error\n");

	CloseHandle(hIOCP);

	WSACleanup();
}

int CLanServer::GetClientCount(){ return _iSessionCount; }

//-------------------------------------------------------------------------------------
// Packet
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
// ��Ŷ ������
//-------------------------------------------------------------------------------------
bool CLanServer::SendPacket(__int64 iSessionID, CNPacket *pPacket)
{
	int iCnt;

	for (iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		if (Session[iCnt]._iSessionID == iSessionID)
		{
			pPacket->addRef();
			Session[iCnt].SendQ.Put((char *)&pPacket, sizeof(pPacket));
			break;
		}
	}
	
	PRO_BEGIN(L"SendPost");
	SendPost(&Session[iCnt]);
	PRO_END(L"SendPost");

	return true;
}

//-------------------------------------------------------------------------------------
// Recv ���
//-------------------------------------------------------------------------------------
void CLanServer::RecvPost(SESSION *pSession, bool bAcceptRecv)
{
	int retval;
	DWORD dwRecvSize, dwflag = 0;
	WSABUF wBuf;

	wBuf.buf = pSession->RecvQ.GetWriteBufferPtr();
	wBuf.len = pSession->RecvQ.GetNotBrokenPutSize();

	if (!bAcceptRecv)
		InterlockedIncrement64((LONG64 *)&(pSession->_lIOCount));

	retval = WSARecv(pSession->_SessionInfo._socket, &wBuf, 1, &dwRecvSize, &dwflag, &pSession->_RecvOverlapped, NULL);

	if (retval == SOCKET_ERROR)
	{
		int iErrorCode = GetLastError();
		if (iErrorCode != WSA_IO_PENDING)
		{
			if (iErrorCode != 10054)
				OnError(iErrorCode, L"RecvPost Error\n");
			
			if (0 == InterlockedDecrement64((LONG64 *)&(pSession->_lIOCount)))
				ReleaseSession(pSession);

			return;
		}
	}
}

//-------------------------------------------------------------------------------------
// Send ���
//-------------------------------------------------------------------------------------
BOOL CLanServer::SendPost(SESSION *pSession)
{
	int retval, iCount = 0;
	DWORD dwSendSize, dwflag = 0;
	WSABUF wBuf[MAX_WSABUF];
	CNPacket *pPacket = NULL;

	char *SendQReadPos = NULL;
	char *SendQWritePos = NULL;
	int SendQUseSize = 0;

	if ((true == InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, (LONG)true, (LONG)true)) ||
		(pSession->_iSendPacketCnt != 0))
		return FALSE;

	do
	{
		////////////////////////////////////////////////////////////////////////////////
		// SendFlag -> true
		////////////////////////////////////////////////////////////////////////////////
		InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, (LONG)true, (LONG)false);
		
		////////////////////////////////////////////////////////////////////////////////
		// ������ SendQ Read, Write, size ���
		////////////////////////////////////////////////////////////////////////////////
		do
		{
			SendQReadPos = pSession->SendQ.GetReadBufferPtr();
			SendQWritePos = pSession->SendQ.GetWriteBufferPtr();
			SendQUseSize = pSession->SendQ.GetUseSize();
		} while ((LONG)SendQReadPos != InterlockedCompareExchange((LONG *)&SendQReadPos, (LONG)SendQReadPos, (LONG)pSession->SendQ.GetReadBufferPtr()));

		if (SendQUseSize == 0)
		{
			////////////////////////////////////////////////////////////////////////////////
			// SendFlag -> false
			////////////////////////////////////////////////////////////////////////////////
			InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, (LONG)false, (LONG)true);
			if (pSession->SendQ.GetUseSize() > 0)
				continue;
		}

		////////////////////////////////////////////////////////////////////////////////
		// WSABUF�� Packet �ֱ�
		////////////////////////////////////////////////////////////////////////////////
		while (pSession->SendQ.GetUseSize() / sizeof(char *) > pSession->_iSendPacketCnt)
		{
			if (iCount >= MAX_WSABUF)
				break;

			pSession->SendQ.Peek((char *)&pPacket, pSession->_iSendPacketCnt * sizeof(char *), sizeof(char *));

			wBuf[iCount].buf = (char *)pPacket->GetHeaderBufferPtr();
			wBuf[iCount].len = pPacket->GetPacketSize();

			iCount++;
			InterlockedIncrement64((LONG64 *)&pSession->_iSendPacketCnt);
		}

		InterlockedIncrement64((LONG64 *)&pSession->_lIOCount);

		PRO_BEGIN(L"WSASend Call");
		retval = WSASend(pSession->_SessionInfo._socket, wBuf, pSession->_iSendPacketCnt, &dwSendSize, dwflag, &pSession->_SendOverlapped, NULL);
		PRO_END(L"WSASend Call");

		if (retval == SOCKET_ERROR)
		{
			int iErrorCode = GetLastError();
			if (iErrorCode != WSA_IO_PENDING)
			{
				if (iErrorCode != 10054)
					OnError(iErrorCode, L"SendPost Error\n");

				if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
					ReleaseSession(pSession);

				return FALSE;
			}
		}
	} while (0);
	return TRUE;
}

bool CLanServer::CompleteRecv(SESSION *pSession, CNPacket *pPacket)
{
	short header;

	if (pSession->RecvQ.GetUseSize() <= sizeof(header))
		return false;
	pSession->RecvQ.Peek((char *)&header, sizeof(header));

	if (pSession->RecvQ.GetUseSize() < sizeof(header) + header)
		return false;

	pPacket->Put(pSession->RecvQ.GetReadBufferPtr(), sizeof(header));
	pSession->RecvQ.RemoveData(sizeof(header));

	pPacket->Put(pSession->RecvQ.GetReadBufferPtr(), header);
	pSession->RecvQ.RemoveData(header);

	return true;
}

//-------------------------------------------------------------------------------------
// Thread
//-------------------------------------------------------------------------------------
int CLanServer::WorkerThread_Update()
{
	int retval;
	CNPacket* pPacket;

	while (!_bShutdown)
	{
		DWORD dwTransferred = 0;
		OVERLAPPED *pOverlapped = NULL;
		SESSION *pSession = NULL;

		PRO_BEGIN(L"GQCS IOComplete");
		retval = GetQueuedCompletionStatus(hIOCP, &dwTransferred, (PULONG_PTR)&pSession,
			(LPOVERLAPPED *)&pOverlapped, INFINITE);
		PRO_END(L"GQCS IOComplete");

		//----------------------------------------------------------------------------
		// Error, ���� ó��
		//----------------------------------------------------------------------------
		// IOCP ���� ���� ����
		if (retval == FALSE && (pOverlapped == NULL || pSession == NULL))
		{
			int iErrorCode = WSAGetLastError();
			OnError(iErrorCode, L"IOCP HANDLE Error\n");

			break;
		}

		// ��Ŀ������ ���� ����
		else if (dwTransferred == 0 && pSession == NULL && pOverlapped == NULL)
		{
			OnError(0, L"Worker Thread Done.\n");
			PostQueuedCompletionStatus(hIOCP, 0, NULL, NULL);
			return 0;
		}

		//----------------------------------------------------------------------------
		// ��������
		// Ŭ���̾�Ʈ ���� closesocket() Ȥ�� shutdown() �Լ��� ȣ���� ����
		//----------------------------------------------------------------------------
		else if (dwTransferred == 0)
		{
			if (pOverlapped == &(pSession->_RecvOverlapped))
			{
				int i = 0;
			}

			else if (pOverlapped == &(pSession->_SendOverlapped))
				pSession->_bSendFlag = false;
				
			if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
				ReleaseSession(pSession);

			continue;
		}
		//----------------------------------------------------------------------------

		OnWorkerThreadBegin();

		//////////////////////////////////////////////////////////////////////////////
		// Recv �Ϸ�
		//////////////////////////////////////////////////////////////////////////////
		if (pOverlapped == &pSession->_RecvOverlapped)
		{    
			pSession->RecvQ.MoveWritePos(dwTransferred);

			PRO_BEGIN(L"PacketAlloc");
			//CNPacket *pPacket = CNPacket::Alloc();
			CNPacket *pPacket = new CNPacket();

			pPacket->iUsedSession = pSession->_iSessionID;
			pPacket->bUse = true;
			pPacket->iStatus = 1;
			pPacket->iAddress = (__int64)pPacket;

			PRO_END(L"PacketAlloc");

			while (1)
			{
				if (!CompleteRecv(pSession, pPacket))
					break;

				InterlockedIncrement64((LONG64 *)&_RecvPacketCounter);
				OnRecv(pSession->_iSessionID, pPacket);
			}

			PRO_BEGIN(L"PacketFree");
			delete pPacket;
			//pPacket->Free();
			PRO_END(L"PacketFree");
			
			RecvPost(pSession);
		}

		//////////////////////////////////////////////////////////////////////////////
		// Send �Ϸ�
		//////////////////////////////////////////////////////////////////////////////
		else if (pOverlapped == &pSession->_SendOverlapped)
		{
			CNPacket *pPacket = NULL;

			for (int iCnt = 0; iCnt < pSession->_iSendPacketCnt; iCnt++)
			{
				pSession->SendQ.Get((char *)&pPacket, sizeof(char *));
				//pPacket->Free();

				pPacket->iUsedSession = pSession->_iSessionID;
				pPacket->bUse = false;
				pPacket->iStatus = 3;
				pPacket->iAddress = (__int64)pPacket;

				delete pPacket;
				InterlockedDecrement64((LONG64 *)&pSession->_iSendPacketCnt);
			}

			//pSession->_iSendPacketCnt = 0;
			InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, (LONG)false, (LONG)true);
			OnSend(pSession->_iSessionID, dwTransferred);

			if (pSession->SendQ.GetUseSize() > 0)
				SendPost(pSession);
		}

		if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
			ReleaseSession(pSession);

		//Count�� 0���� ������ ũ���� ����
		else if (0 > pSession->_lIOCount)
			CCrashDump::Crash();
			
		OnWorkerThreadEnd();
	}

	return 0;
}

int CLanServer::AcceptThread_Update()
{
	HANDLE retval;

	SOCKET ClientSocket;
	int addrlen = sizeof(SOCKADDR_IN);
	SOCKADDR_IN clientSock;
	WCHAR clientIP[16];

	while (!_bShutdown)
	{
		ClientSocket = WSAAccept(listen_sock, (SOCKADDR *)&clientSock, &addrlen, NULL, NULL);

		if (ClientSocket == INVALID_SOCKET)
		{
			DisconnectSession(ClientSocket);
			continue;
		}
		InetNtop(AF_INET, &clientSock.sin_addr, clientIP, 16);

		/*
		if (_iSessionCount >= MAX_SESSION)
			OnError(dfMAX_SESSION, L"Session is Maximun!");
			*/

		if (!OnConnectionRequest(clientIP, ntohs(clientSock.sin_port)))		// accept ����
		{
			DisconnectSession(ClientSocket);
			continue;
		}	
		InterlockedIncrement64((LONG64 *)&_AcceptCounter);
		InterlockedIncrement64((LONG64 *)&_AcceptTotalCounter);

		//////////////////////////////////////////////////////////////////////////////
		// ���� �߰� ����
		//////////////////////////////////////////////////////////////////////////////
		for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
		{
			// �� ����
			if (!Session[iCnt]._bUsed)
			{
				Session[iCnt]._bUsed = true;

				/////////////////////////////////////////////////////////////////////
				// ���� �ʱ�ȭ
				/////////////////////////////////////////////////////////////////////
				wcscpy_s(Session[iCnt]._SessionInfo._IP, 16, clientIP);
				Session[iCnt]._SessionInfo._iPort = ntohs(clientSock.sin_port);

				/////////////////////////////////////////////////////////////////////
				// KeepAlive
				/////////////////////////////////////////////////////////////////////
				tcp_keepalive tcpkl;

				tcpkl.onoff = 1;
				tcpkl.keepalivetime = 3000; //30�� �����Ҷ� ª�� ���̺궩 2~30��
				tcpkl.keepaliveinterval = 2000; //  keepalive ��ȣ

				DWORD dwReturnByte;
				WSAIoctl(ClientSocket, SIO_KEEPALIVE_VALS, &tcpkl, sizeof(tcp_keepalive), 0,
					0, &dwReturnByte, NULL, NULL);
				/////////////////////////////////////////////////////////////////////

				Session[iCnt]._SessionInfo._socket = ClientSocket;

				Session[iCnt]._iSessionID = InterlockedIncrement64((LONG64 *)&_iSessionID);

				Session[iCnt].RecvQ.ClearBuffer();
				Session[iCnt].SendQ.ClearBuffer();

				memset(&(Session[iCnt]._RecvOverlapped), 0, sizeof(OVERLAPPED));
				memset(&(Session[iCnt]._SendOverlapped), 0, sizeof(OVERLAPPED));

				Session[iCnt]._bSendFlag = FALSE;
				Session[iCnt]._lIOCount = 0;

				retval = CreateIoCompletionPort((HANDLE)Session[iCnt]._SessionInfo._socket,
					hIOCP, 
					(ULONG_PTR)&Session[iCnt], 
					0);

				if (!retval)
					PostQueuedCompletionStatus(hIOCP, 0, 0, 0);
				
				InterlockedIncrement64((LONG64 *)&Session[iCnt]._lIOCount);
				OnClientJoin(&Session[iCnt]._SessionInfo, Session[iCnt]._iSessionID);

				PRO_BEGIN(L"RecvPost(1st)");
				RecvPost(&Session[iCnt], true);
				PRO_END(L"RecvPost(1st)");

				InterlockedIncrement64((LONG64 *)&_iSessionCount);
				break;
			}
		}
	}

	return 0;
}

int CLanServer::MonitorThread_Update()
{
	DWORD iGetTime = timeGetTime();

	while (1)
	{
		if (timeGetTime() - iGetTime < 1000)
			continue;

		_AcceptTPS = _AcceptCounter;
		_AcceptTotalTPS += _AcceptTotalCounter;
		_RecvPacketTPS = _RecvPacketCounter;
		_SendPacketTPS = _SendPacketCounter;
		_PacketPoolTPS = CNPacket::GetPacketPoolCount();

		_AcceptCounter = 0;
		_AcceptTotalCounter = 0;
		_RecvPacketCounter = 0;
		_SendPacketCounter = 0;

		iGetTime = timeGetTime();
	}
}

unsigned __stdcall CLanServer::WorkerThread(LPVOID workerArg)
{
	return ((CLanServer *)workerArg)->WorkerThread_Update();
}

unsigned __stdcall CLanServer::AcceptThread(LPVOID acceptArg)
{
	return ((CLanServer *)acceptArg)->AcceptThread_Update();
}

unsigned __stdcall CLanServer::MonitorThread(LPVOID monitorArg)
{
	return ((CLanServer *)monitorArg)->MonitorThread_Update();
}

//-------------------------------------------------------------------------------------
// Disconnect
//-------------------------------------------------------------------------------------
void CLanServer::DisconnectSession(SOCKET socket)
{
	shutdown(socket, SD_BOTH);
}

void CLanServer::DisconnectSession(SESSION *pSession)
{
	DisconnectSession(pSession->_SessionInfo._socket);
}

void CLanServer::DisconnectSession(__int64 iSessionID)
{
	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		if (Session[iCnt]._iSessionID == iSessionID)
		{
			DisconnectSession(&Session[iCnt]);
			break;
		}
	}
}

//-------------------------------------------------------------------------------------
// Release
//-------------------------------------------------------------------------------------
void CLanServer::ReleaseSession(SESSION *pSession)
{
	DisconnectSession(pSession);

	pSession->_bUsed = false;
	InterlockedDecrement64((LONG64 *)&_iSessionCount);
}

void CLanServer::ReleaseSession(__int64 iSessionID)
{
	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		if (Session[iCnt]._iSessionID == iSessionID)
		{
			ReleaseSession(&Session[iCnt]);
			break;
		}
	}
}