#pragma once
#ifndef SOCKETS_HPP
#define SOCKETS_HPP

#include "Shared.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <winsock.h>
#include <Ws2tcpip.h>
#include <mstcpip.h>
#define WIN(exp) exp
#define NIX(exp)
#else
#define SD_BOTH 0
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#define WIN(exp)
#define NIX(exp) exp
#endif

namespace SocketTCP {

#ifdef _WIN32
	inline i32 ConvertError() {
		i32 lastError = WSAGetLastError();
		switch (lastError) {
		case 0:
			return 0;
		case WSAEINTR:
			return EINTR;
		case WSAEINVAL:
			return EINVAL;
		case WSA_INVALID_HANDLE:
			return EBADF;
		case WSA_NOT_ENOUGH_MEMORY:
			return ENOMEM;
		case WSA_INVALID_PARAMETER:
			return EINVAL;
		case WSAENAMETOOLONG:
			return ENAMETOOLONG;
		case WSAENOTEMPTY:
			return ENOTEMPTY;
		case WSAEWOULDBLOCK:
			return EAGAIN;
		case WSAEINPROGRESS:
			return EINPROGRESS;
		case WSAEALREADY:
			return EALREADY;
		case WSAENOTSOCK:
			return ENOTSOCK;
		case WSAEDESTADDRREQ:
			return EDESTADDRREQ;
		case WSAEMSGSIZE:
			return EMSGSIZE;
		case WSAEPROTOTYPE:
			return EPROTOTYPE;
		case WSAENOPROTOOPT:
			return ENOPROTOOPT;
		case WSAEPROTONOSUPPORT:
			return EPROTONOSUPPORT;
		case WSAEOPNOTSUPP:
			return EOPNOTSUPP;
		case WSAEAFNOSUPPORT:
			return EAFNOSUPPORT;
		case WSAEADDRINUSE:
			return EADDRINUSE;
		case WSAEADDRNOTAVAIL:
			return EADDRNOTAVAIL;
		case WSAENETDOWN:
			return ENETDOWN;
		case WSAENETUNREACH:
			return ENETUNREACH;
		case WSAENETRESET:
			return ENETRESET;
		case WSAECONNABORTED:
			return ECONNABORTED;
		case WSAECONNRESET:
			return ECONNRESET;
		case WSAENOBUFS:
			return ENOBUFS;
		case WSAEISCONN:
			return EISCONN;
		case WSAENOTCONN:
			return ENOTCONN;
		case WSAETIMEDOUT:
			return ETIMEDOUT;
		case WSAECONNREFUSED:
			return ECONNREFUSED;
		case WSAELOOP:
			return ELOOP;
		case WSAEHOSTUNREACH:
			return EHOSTUNREACH;
		default:
			return EIO;
		}
	}

	class WinSocketInit {
	public:

		WSAData _wData;

		WinSocketInit() {
			CheckWSAStartup(WSAStartup(MAKEWORD(2, 2), &_wData));
		}

		~WinSocketInit() {
			WSACleanup();
		}

		static void CheckWSAStartup(i32 result) {
			if (result != 0)
				throw std::runtime_error(std::format("WSAStartup error #{}", result));
		}
	};

	static inline WinSocketInit kWinsockInit{};

	using SockLen_t = i32;
	using SocketAddr_in = SOCKADDR_IN;
	using Socket = SOCKET;
	using KaPropertyType = u64;
#else
	using SockLen_t = socklen_t;
	using SocketAddr_in = struct sockaddr_in;
	using Socket = i32;
	using KaPropertyType = i32;
#endif
	
	using DataBuffer = std::vector<u8>;
	
	enum class ClientSocketStatus : u8 {
		kConnected,
		kErrSocketInit,
		kErrSocketBind,
		kErrSocketConnect,
		kDisconnected
	};

	enum class RecvResult {
		kOk,
		kAgain,
		kDisconnect
	};

	inline string U32ToIpAddress(u32 ip)
	{
		if (ip == 0)
			return "0.0.0.0 (any)";
		sstream result;
		result
			<< u32{ static_cast<u8>(reinterpret_cast<char*>(&ip)[0]) } << '.'
			<< u32{ static_cast<u8>(reinterpret_cast<char*>(&ip)[1]) } << '.'
			<< u32{ static_cast<u8>(reinterpret_cast<char*>(&ip)[2]) } << '.'
			<< u32{ static_cast<u8>(reinterpret_cast<char*>(&ip)[3]) };
		return result.str();
	}

	class ISocketCalls {
	public:
		virtual ~ISocketCalls() = default;
		virtual Socket NewSocket(i32 af, i32 type, i32 proto) = 0;
		virtual i32 SetSockOptions(Socket s, i32 level, i32 opname, char* optval, u32 optsize) = 0;
		virtual i32 Connect(Socket s, SocketAddr_in& address) = 0;
		virtual i32 Close(Socket s) = 0;
		virtual i32 Shutdown(Socket s, i32 how) = 0;
		virtual RecvResult Recv(Socket s, DataBuffer* data, u64 bytesToRead, i32 flags) = 0;
		virtual RecvResult CheckRecv(Socket s, i32 answer) = 0;
		virtual i32 Send(Socket s, DataBuffer& data, i32 flags) = 0;
		virtual i32 Bind(Socket s, SocketAddr_in& address) = 0;
		virtual i32 Listen(Socket s, i32 backlog) = 0;
		virtual i32 SelectBeforeAccept(Socket s, fd_set* readfds, const timeval* timeout) = 0;
		virtual Socket Accept(Socket s, SocketAddr_in& addr) = 0;
		virtual void FdSet(Socket s, fd_set* set) = 0;
		virtual bool FdIsSet(Socket s, fd_set* set) = 0;

#ifdef _WIN32
		virtual i32 wsaioctl(Socket s, u32 dwIoControlCode,
			void* lpvInBuffer, u32 cbInBuffer, void* lpvOutBuffer,
			u32 cbOutBuffer, u32* lpcbBytesReturned, OVERLAPPED* lpOverlapped,
			LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = 0;

		virtual i32 IOctlSocket(Socket s, i32 cmd, u32* argp) = 0;
#endif
	};

	class SocketCalls : public ISocketCalls {
	public:
		Socket NewSocket(i32 af, i32 type, i32 proto) override;
		i32 SetSockOptions(Socket s, i32 level, i32 opname, char * optval, u32 optsize) override;
		i32 Connect(Socket s, SocketAddr_in& address) override;
		i32 Close(Socket s) override;
		i32 Shutdown(Socket s, i32 how) override;
		RecvResult Recv(Socket s, DataBuffer* data, u64 bytesToRead, i32 flags) override;
		RecvResult CheckRecv(Socket s, i32 answer) override;
		i32 Send(Socket s, DataBuffer& data, i32 flags) override;
		i32 Bind(Socket s, SocketAddr_in& address) override;
		i32 Listen(Socket s, i32 backlog) override;
		i32 SelectBeforeAccept(Socket s, fd_set* readfds, const timeval* timeout) override;
		Socket Accept(Socket s, SocketAddr_in& addr) override;
		void FdSet(Socket s, fd_set* set) override;
		bool FdIsSet(Socket s, fd_set* set) override;

#ifdef _WIN32
		i32 wsaioctl(Socket s, u32 dwIoControlCode,
			void* lpvInBuffer, u32 cbInBuffer, void* lpvOutBuffer,
			u32 cbOutBuffer, u32* lpcbBytesReturned, OVERLAPPED* lpOverlapped,
			LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) override;

		i32 IOctlSocket(Socket s, i32 cmd, u32* argp) override;
#endif
	};

	inline bool GetIPAddress(string value, i64& result) {
		return inet_pton(AF_INET, value.c_str(), &result) == 1;
	}

	inline bool GetPort(string value, i64& result) {
		return GetNumericValue(value, result, 0, 0xFFFF);
	}
}

#endif // !SOCKETS_HPP
