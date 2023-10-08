#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "socketCallsMock.hpp"

#include "../Client.hpp"
#include "../Client.cpp"

using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SaveArg;
using ::testing::Return;
using ::testing::ElementsAre;
using ::testing::_;

namespace SocketTCP {
	
	namespace Client {
		
		class ClientTest : public ::testing::Test {
		public:
			Client* client_ = nullptr;
			SocketCallsMock* socket_calls_mock_ = nullptr;
			const u64 kWaitingTime = 200; // ms

 			void SetUp() { client_ = new Client(); socket_calls_mock_ = new SocketCallsMock(); client_->sockets = socket_calls_mock_; }
			void TearDown() { delete socket_calls_mock_; delete client_; }
		};

		TEST_F(ClientTest, InvalidNewSocket) {
			EXPECT_CALL(*socket_calls_mock_, NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP)).Times(1).WillOnce(Return(-1));
			ASSERT_EQ(client_->connectTo(0xFFFFFFFF, 1), ClientSocketStatus::kErrSocketInit);
		}

		TEST_F(ClientTest, NoCorrectConnect) {
			Socket actualSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
			EXPECT_CALL(*socket_calls_mock_, NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP))
				.Times(1).WillOnce(Return(actualSocket));
			EXPECT_CALL(*socket_calls_mock_, SetSockOptions(actualSocket, SOL_SOCKET, SO_RCVTIMEO, _, _))
				.Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Connect(actualSocket, _)).Times(1).WillOnce(Return(-1));

			EXPECT_EQ(client_->connectTo(0x0100007F, 1000), ClientSocketStatus::kErrSocketConnect);
			EXPECT_EQ(client_->loadData().size(), 0);
			EXPECT_EQ(client_->sendData("abc"), false);
			EXPECT_EQ(client_->disconnect(), ClientSocketStatus::kErrSocketConnect);

			if (actualSocket != WIN(INVALID_SOCKET)NIX(-1))
				WIN(closesocket)NIX(close)(actualSocket);
		}

		TEST_F(ClientTest, CorrectProcess) {

			DataBuffer sendBuffer = DataBuffer(256);

			Socket actualSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
			EXPECT_CALL(*socket_calls_mock_, NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP))
				.Times(1).WillOnce(Return(actualSocket));
			EXPECT_CALL(*socket_calls_mock_, SetSockOptions(actualSocket, SOL_SOCKET, SO_RCVTIMEO, _, _))
				.Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Connect(actualSocket, _)).Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Send(actualSocket, _, _)).Times(1).WillOnce(Return(-1));
			EXPECT_CALL(*socket_calls_mock_, Send(actualSocket, ElementsAre(4, 0, 0, 0, 0, 0, 0, 0, 'a', 'b', 'c', 0), _))
				.Times(1).WillOnce(Return(0)).RetiresOnSaturation();
			EXPECT_CALL(*socket_calls_mock_, Recv(actualSocket, _, _, _))
				.Times(3).WillOnce(Return(RecvResult::kOk)).WillOnce(Return(RecvResult::kAgain)).WillOnce(Return(RecvResult::kDisconnect));
			EXPECT_CALL(*socket_calls_mock_, Shutdown(actualSocket, SD_BOTH)).Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Close(actualSocket)).Times(1).WillOnce(Return(0));

			EXPECT_EQ(client_->connectTo(0x0100007F, 1000), ClientSocketStatus::kConnected);
			EXPECT_EQ(client_->getHost(), 0x0100007F);
			EXPECT_EQ(client_->getPort(), htons(1000));
			EXPECT_EQ(client_->sendData("abc"), true);
			EXPECT_EQ(client_->sendData("def"), false);
			EXPECT_EQ(client_->loadData().size(), 0);
			EXPECT_EQ(client_->loadData().size(), 0);
			EXPECT_EQ(client_->loadData().size(), 0);
			EXPECT_EQ(client_->getStatus(), ClientSocketStatus::kDisconnected);

			if (actualSocket != WIN(INVALID_SOCKET)NIX(-1))
				WIN(closesocket)NIX(close)(actualSocket);
		}

		TEST_F(ClientTest, LoadData) {
			Socket actualSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
			EXPECT_CALL(*socket_calls_mock_, NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP))
				.Times(1).WillOnce(Return(actualSocket));
			EXPECT_CALL(*socket_calls_mock_, SetSockOptions(actualSocket, SOL_SOCKET, SO_RCVTIMEO, _, _))
				.Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Connect(actualSocket, _)).Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Recv(actualSocket, _, _, _))
				.Times(6)
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{10, 0, 0, 0}), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 10, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(Return(RecvResult::kAgain))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 10, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(Return(RecvResult::kDisconnect));
			EXPECT_CALL(*socket_calls_mock_, Shutdown(actualSocket, SD_BOTH)).Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Close(actualSocket)).Times(1).WillOnce(Return(0));

			EXPECT_EQ(client_->connectTo(0x0100007F, 1000), ClientSocketStatus::kConnected);
			EXPECT_THAT(client_->loadData(), ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
			EXPECT_EQ(client_->loadData().size(), 0);
			EXPECT_EQ(client_->loadData().size(), 0);
			EXPECT_EQ(client_->getStatus(), ClientSocketStatus::kDisconnected);
			
			if (actualSocket != WIN(INVALID_SOCKET)NIX(-1))
				WIN(closesocket)NIX(close)(actualSocket);
		}

		TEST_F(ClientTest, HandlerTest) {
			ASSERT_EQ(client_->setHandler([](DataBuffer data) {}), true);
			ASSERT_NO_THROW(client_->joinHandler());

			Socket actualSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
			EXPECT_CALL(*socket_calls_mock_, NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP))
				.Times(1).WillOnce(Return(actualSocket));
			EXPECT_CALL(*socket_calls_mock_, SetSockOptions(actualSocket, SOL_SOCKET, SO_RCVTIMEO, _, _))
				.Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Connect(actualSocket, _)).Times(1).WillOnce(Return(0));
			EXPECT_CALL(*socket_calls_mock_, Recv(actualSocket, _, _, _))
				.Times(2)
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 10, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }), Return(RecvResult::kOk)));
			EXPECT_CALL(*socket_calls_mock_, Shutdown(actualSocket, SD_BOTH)).Times(1);
			EXPECT_CALL(*socket_calls_mock_, Close(actualSocket)).Times(1);
			
			EXPECT_EQ(client_->connectTo(0x0100007F, 1000), ClientSocketStatus::kConnected);

			u64 test_value = 0;

			ASSERT_NO_THROW(client_->setHandler([&test_value](DataBuffer data) { test_value = 1; throw std::runtime_error("Test error"); }));
			
			auto start = std::chrono::high_resolution_clock::now();

			while (test_value != 1 and
				(std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start)).count() < kWaitingTime) {}
			
			ASSERT_NO_THROW(client_->joinHandler());
			EXPECT_EQ(client_->disconnect(), ClientSocketStatus::kDisconnected);

			if (actualSocket != WIN(INVALID_SOCKET)NIX(-1))
				WIN(closesocket)NIX(close)(actualSocket);
		}
	}
}