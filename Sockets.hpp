#pragma once
#ifndef SOCKETS_HPP
#define SOCKETS_HPP

#include "Shared.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <winsock.h>
#include <Ws2tcpip.h>
#include <mstcpip.h>
#else
#define SD_BOTH 0
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#endif

namespace SocketTCP {

#ifdef _WIN32
#define WIN(exp) exp
#define NIX(exp)
	inline i32 convertError()
	{
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
#else
#define WIN(exp)
#define NIX(exp) exp
#endif

#ifdef _WIN32
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

	constexpr u32 kLoopBack = 0x0100007f;

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

	typedef std::vector<u8> DataBuffer;

	enum class SocketType : u8 {
		kClientSocket = 0,
		kServerSocket = 1
	};

#ifdef _WIN32
	namespace WinInit {
		class _WinSocketInit {
		public:
			
			WSAData _wData;

			_WinSocketInit() {
				i32 result = WSAStartup(MAKEWORD(2, 2), &_wData);
				if (result != 0)
					throw std::runtime_error(std::format("WSAStartup error #{}", result));
			}

			~_WinSocketInit() {
				WSACleanup();
			}
		};
		
		static inline _WinSocketInit _winsockInit;
	}
#endif

	inline string U32ToIpAddress(u32 ip)
	{
		if (ip == 0)
			return "0.0.0.0 (any)";
		sstream result;
		result
			<< i32{ reinterpret_cast<char*>(&ip)[0] } << '.'
			<< i32{ reinterpret_cast<char*>(&ip)[1] } << '.'
			<< i32{ reinterpret_cast<char*>(&ip)[2] } << '.'
			<< i32{ reinterpret_cast<char*>(&ip)[3] };
		return result.str();
	}
}

#endif // !SOCKETS_HPP
