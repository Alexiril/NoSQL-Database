#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "../Sockets.hpp"
#include "../Sockets.cpp"

namespace SocketTCP
{

	static ISocketCalls *sockets = new SocketCalls();

	class SocketCallsTest : public ::testing::Test
	{
	public:
		SocketCalls calls;

		void SetUp() { calls = SocketCalls(); }
	};

	TEST(GetIP, StandardUsing)
	{
		i64 test_value;
		ASSERT_EQ(GetIPAddress("127.0.0.1", test_value), true);
		ASSERT_EQ(static_cast<i32>(test_value), 0x0100007F);
	}

	TEST(GetPort, StandardUsing)
	{
		i64 test_value;
		ASSERT_EQ(GetPort("7000", test_value), true);
		ASSERT_EQ(test_value, 7000);
	}

#ifdef _WIN32

	TEST(ConvertError, StandardErrors)
	{
		std::map<i32, i32> variants = {
			{0, 0},
			{WSAEINTR, EINTR},
			{WSAEINVAL, EINVAL},
			{WSA_INVALID_HANDLE, EBADF},
			{WSA_NOT_ENOUGH_MEMORY, ENOMEM},
			{WSA_INVALID_PARAMETER, EINVAL},
			{WSAENAMETOOLONG, ENAMETOOLONG},
			{WSAENOTEMPTY, ENOTEMPTY},
			{WSAEWOULDBLOCK, EAGAIN},
			{WSAEINPROGRESS, EINPROGRESS},
			{WSAEALREADY, EALREADY},
			{WSAENOTSOCK, ENOTSOCK},
			{WSAEDESTADDRREQ, EDESTADDRREQ},
			{WSAEMSGSIZE, EMSGSIZE},
			{WSAEPROTOTYPE, EPROTOTYPE},
			{WSAENOPROTOOPT, ENOPROTOOPT},
			{WSAEPROTONOSUPPORT, EPROTONOSUPPORT},
			{WSAEOPNOTSUPP, EOPNOTSUPP},
			{WSAEAFNOSUPPORT, EAFNOSUPPORT},
			{WSAEADDRINUSE, EADDRINUSE},
			{WSAEADDRNOTAVAIL, EADDRNOTAVAIL},
			{WSAENETDOWN, ENETDOWN},
			{WSAENETUNREACH, ENETUNREACH},
			{WSAENETRESET, ENETRESET},
			{WSAECONNABORTED, ECONNABORTED},
			{WSAECONNRESET, ECONNRESET},
			{WSAENOBUFS, ENOBUFS},
			{WSAEISCONN, EISCONN},
			{WSAENOTCONN, ENOTCONN},
			{WSAETIMEDOUT, ETIMEDOUT},
			{WSAECONNREFUSED, ECONNREFUSED},
			{WSAELOOP, ELOOP},
			{WSAEHOSTUNREACH, EHOSTUNREACH}};

		for (const auto &[task, answer] : variants)
		{
			WSASetLastError(task);
			ASSERT_EQ(ConvertError(), answer);
		}
	}

	TEST(ConvertError, UnexpectedValues)
	{
		WSASetLastError(100);
		ASSERT_EQ(ConvertError(), EIO);
	}

	TEST(WinSocketInit, CheckWSAStartup)
	{
		ASSERT_NO_THROW(WinSocketInit::CheckWSAStartup(0));
		ASSERT_THROW(WinSocketInit::CheckWSAStartup(1), std::runtime_error);
	}

	TEST(WinSocketInit, FullProcess)
	{
		WinSocketInit *a = nullptr;
		ASSERT_NO_THROW(a = new WinSocketInit());
		ASSERT_NO_THROW(delete a);
	}

	TEST_F(SocketCallsTest, WSAIoctl)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);

		EXPECT_NE(calls.wsaioctl(socket, SIO_KEEPALIVE_VALS, nullptr, 0, nullptr, 0, nullptr, 0, nullptr), 0);

		calls.Close(socket);
	}

	TEST_F(SocketCallsTest, IOctlSocket)
	{
		ASSERT_NE(calls.IOctlSocket(0, 0, 0), 0);
	}

#endif

	TEST(U32ToIpAddress, StandardUsing)
	{
		std::map<u32, string> variants = {
			{0, "0.0.0.0 (any)"},
			{0xFFFFFFFF, "255.255.255.255"},
			{0x0100007F, "127.0.0.1"},
			{0x0100A8C0, "192.168.0.1"}};

		for (const auto &[task, answer] : variants)
			ASSERT_EQ(U32ToIpAddress(task), answer);
	}

	TEST_F(SocketCallsTest, NewSocket)
	{
		ASSERT_GT(calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP), 0);
		ASSERT_LE(static_cast<i64>(calls.NewSocket(0, 0, 0)), 0);
	}

	TEST_F(SocketCallsTest, SetSockOptions)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		EXPECT_NE(calls.SetSockOptions(socket, 0, 0, 0, 0), 0);

		u32 tv = 1;
		EXPECT_EQ(sockets->SetSockOptions(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&tv), sizeof(tv)), WIN(0) NIX(-1));

		WIN(closesocket)
		NIX(close)
		(socket);
	}

	TEST_F(SocketCallsTest, Connect)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);

		SocketAddr_in addr{};

		addr.sin_family = AF_INET;

		EXPECT_NE(calls.Connect(socket, addr), 0);

		WIN(closesocket)
		NIX(close)
		(socket);
	}

	TEST_F(SocketCallsTest, Close)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		ASSERT_EQ(calls.Close(socket), 0);
	}

	TEST_F(SocketCallsTest, Shutdown)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		EXPECT_EQ(calls.Shutdown(socket, SD_BOTH), -1);
		calls.Close(socket);
	}

	TEST_F(SocketCallsTest, Send)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		DataBuffer data = {1, 1, 1};
#ifdef _WIN32
		EXPECT_EQ(calls.Send(socket, data, 0), -1);
#else
		EXPECT_ANY_THROW(calls.Send(socket, data, 0));
#endif
		calls.Close(socket);
	}

	TEST_F(SocketCallsTest, Bind)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		SocketAddr_in addr{};
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		addr.sin_port = htons(10001);
		EXPECT_EQ(calls.Bind(socket, addr), 0);
		calls.Close(socket);
	}

	TEST_F(SocketCallsTest, Listen)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		EXPECT_NE(calls.Listen(socket, SOMAXCONN), 0);
		calls.Close(socket);
	}

	TEST_F(SocketCallsTest, Accept)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		SocketAddr_in addr{};
		EXPECT_NE(calls.Accept(socket, addr), 0);
		calls.Close(socket);
	}

	TEST_F(SocketCallsTest, SelectBeforeAccept)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		EXPECT_NE(calls.SelectBeforeAccept(socket, nullptr, nullptr), 0);
		calls.Close(socket);
	}

	TEST_F(SocketCallsTest, CheckRecv)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		EXPECT_EQ(calls.CheckRecv(socket, 1), RecvResult::kOk);
		EXPECT_EQ(calls.CheckRecv(socket, 0), RecvResult::kDisconnect);
		EXPECT_EQ(calls.CheckRecv(socket, -1), RecvResult::kOk);
		WIN(WSASetLastError(WSAEWOULDBLOCK))
		NIX(errno = EAGAIN);
		EXPECT_EQ(calls.CheckRecv(socket, -1), RecvResult::kAgain);
		WIN(WSASetLastError(WSAECONNABORTED))
		NIX(errno = ECONNABORTED);
		EXPECT_EQ(calls.CheckRecv(socket, -1), RecvResult::kDisconnect);
		calls.Close(socket);
	}

	TEST_F(SocketCallsTest, Recv)
	{
		Socket socket = calls.NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		EXPECT_EQ(calls.Recv(socket, nullptr, 0, 0), RecvResult::kOk);
		DataBuffer data;
		EXPECT_EQ(calls.Recv(socket, &data, 10, 0), RecvResult::kDisconnect);
		calls.Close(socket);
	}

	TEST_F(SocketCallsTest, FDSetIsset)
	{
		auto s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		fd_set a{};
		ASSERT_NO_THROW(calls.FdSet(s, &a));
		ASSERT_NO_THROW(calls.FdIsSet(s, &a));
	}

}