#pragma once
#ifndef SOCKETCALLSMOCK_HPP
#define SOCKETCALLSMOCK_HPP

#include "gmock/gmock.h"

#include "../Sockets.hpp"

namespace SocketTCP {

	class SocketCallsMock : public ISocketCalls {
	public:
		~SocketCallsMock() override = default;
		MOCK_METHOD(Socket, NewSocket, (i32 af, i32 type, i32 proto), (override));
		MOCK_METHOD(i32, SetSockOptions, (Socket s, i32 level, i32 opname, char *optval, u32 optsize), (override));
		MOCK_METHOD(i32, Connect, (Socket s, SocketAddr_in &address), (override));
		MOCK_METHOD(i32, Close, (Socket s), (override));
		MOCK_METHOD(i32, Shutdown, (Socket s, i32 how), (override));
		MOCK_METHOD(RecvResult, Recv, (Socket s, DataBuffer *data, u64 bytesToRead, i32 flags), (override));
		MOCK_METHOD(RecvResult, CheckRecv, (Socket s, i32 answer), (override));
		MOCK_METHOD(i32, Send, (Socket s, DataBuffer &data, i32 flags), (override));
		MOCK_METHOD(i32, Bind, (Socket s, SocketAddr_in &address), (override));
		MOCK_METHOD(i32, Listen, (Socket s, i32 backlog), (override));
		MOCK_METHOD(i32, SelectBeforeAccept, (Socket s, fd_set *readfds, timeval *timeout), (override));
		MOCK_METHOD(Socket, Accept, (Socket s, SocketAddr_in &addr), (override));
		MOCK_METHOD(void, FdSet, (Socket s, fd_set *set), (override));
		MOCK_METHOD(bool, FdIsSet, (Socket s, fd_set *set), (override));

#ifdef _WIN32
		MOCK_METHOD(i32, wsaioctl, (Socket s, u32 dwIoControlCode, void *lpvInBuffer, u32 cbInBuffer, void *lpvOutBuffer, u32 cbOutBuffer, u32 *lpcbBytesReturned, OVERLAPPED *lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine), (override));
		MOCK_METHOD(i32, IOctlSocket, (Socket s, i32 cmd, u32 *arpg), (override));
#endif
	};
}

#endif // !SOCKETCALLSMOCK_HPP
