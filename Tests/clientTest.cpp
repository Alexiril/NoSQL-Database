#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "../Client.hpp"
#include "../Client.cpp"

using ::testing::Return;
using ::testing::_;

namespace SocketTCP {
	
	class SocketCallsMock : public ISocketCalls {
	public:
		~SocketCallsMock() override = default;
		MOCK_METHOD(Socket, NewSocket, (i32 af, i32 type, i32 proto), (override));
		MOCK_METHOD(i32, SetSockOptions, (Socket s, i32 level, i32 opname, char* optval, u32 optsize), (override));
		MOCK_METHOD(i32, Connect, (Socket s, SocketAddr_in& address), (override));
		MOCK_METHOD(i32, Close, (Socket s), (override));
		MOCK_METHOD(i32, Shutdown, (Socket s, i32 how), (override));
		MOCK_METHOD(RecvResult, Recv, (Socket s, DataBuffer* data, u64 bytesToRead, i32 flags), (override));
		MOCK_METHOD(RecvResult, CheckRecv, (Socket s, i32 answer), (override));
		MOCK_METHOD(i32, Send, (Socket s, DataBuffer& data, i32 flags), (override));
		MOCK_METHOD(i32, Bind, (Socket s, SocketAddr_in& address), (override));
		MOCK_METHOD(i32, Listen, (Socket s, i32 backlog), (override));
		MOCK_METHOD(i32, SelectBeforeAccept, (Socket s, fd_set* readfds, const timeval* timeout), (override));
		MOCK_METHOD(Socket, Accept, (Socket s, SocketAddr_in& addr), (override));

#ifdef _WIN32
		MOCK_METHOD(i32, wsaioctl, (Socket s, u32 dwIoControlCode,
			void* lpvInBuffer, u32 cbInBuffer, void* lpvOutBuffer,
			u32 cbOutBuffer, u32* lpcbBytesReturned, OVERLAPPED* lpOverlapped,
			LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine), (override));
#endif
	};

	
	namespace Client {
		
		class ClientTest : public ::testing::Test {
		public:
			Client* client_ = nullptr;
			SocketCallsMock* socket_calls_mock_ = nullptr;

			void SetUp() { client_ = new Client(); socket_calls_mock_ = new SocketCallsMock(); sockets = socket_calls_mock_; }
			void TearDown() { delete socket_calls_mock_; delete client_; }
		};

		TEST_F(ClientTest, InvalidNewSocket) {
			EXPECT_CALL(*socket_calls_mock_, NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP)).Times(1).WillOnce(Return(-1));
			auto status = client_->connectTo(0xFFFFFFFF, 1);
			ASSERT_EQ(status, ClientSocketStatus::kErrSocketInit);
		}

		TEST_F(ClientTest, NoCorrectConnectNoNeedDisconnect) {
			Socket actualSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
			EXPECT_CALL(*socket_calls_mock_, NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP))
				.Times(1).WillOnce(Return(actualSocket));
			EXPECT_CALL(*socket_calls_mock_, SetSockOptions(actualSocket, SOL_SOCKET, SO_RCVTIMEO, _, _))
				.Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Connect(actualSocket, _)).Times(1).WillOnce(Return(-1));

			EXPECT_EQ(client_->connectTo(0x0100007F, 1000), ClientSocketStatus::kErrSocketConnect);
			EXPECT_EQ(client_->disconnect(), ClientSocketStatus::kErrSocketConnect);
		}

		TEST_F(ClientTest, CorrectConnectDisconnect) {
			Socket actualSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
			EXPECT_CALL(*socket_calls_mock_, NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP))
				.Times(1).WillOnce(Return(actualSocket));
			EXPECT_CALL(*socket_calls_mock_, SetSockOptions(actualSocket, SOL_SOCKET, SO_RCVTIMEO, _, _))
				.Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Connect(actualSocket, _)).Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Shutdown(actualSocket, SD_BOTH)).Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Close(actualSocket)).Times(1).WillOnce(Return(0));

			EXPECT_EQ(client_->connectTo(0x0100007F, 1000), ClientSocketStatus::kConnected);
			EXPECT_EQ(client_->disconnect(), ClientSocketStatus::kDisconnected);
		}

	}
}