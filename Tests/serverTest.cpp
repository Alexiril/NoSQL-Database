#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "socketCallsMock.hpp"

#include "../Server.hpp"
#include "../Server.cpp"

using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SaveArg;
using ::testing::Return;
using ::testing::ElementsAre;
using ::testing::_;

namespace SocketTCP {

	namespace Server {

		static SocketCallsMock socket_calls_mock = SocketCallsMock();

		static Server::HandlerFunctionType handler = [](DataBuffer data, Server::Client& client) {};
		static Server::ConnectionHandlerFunctionType conn = [](Server::Client& client) {};
		static Server::ConnectionHandlerFunctionType disconn = [](Server::Client& client) {};

		class ServerTest : public ::testing::Test {
		public:
			Server* server_ = nullptr;
			const u64 kWaitingTime = 200; // ms

			void SetUp() {
				server_ = new Server(0x0100007F, 1111, { 1, 1, 1 }, handler, conn, disconn, 2);
				server_->sockets = &socket_calls_mock;
			}
			void TearDown() { server_->halt(); delete server_; }
		};

		TEST_F(ServerTest, CheckGetters) {
			EXPECT_EQ(server_->getThreadPool().getThreadCount(), 2);
			EXPECT_EQ(server_->getPort(), 1111);
			EXPECT_EQ(server_->getIpAddress(), 0x0100007F);
			EXPECT_EQ(server_->getStatus(), Server::Status::kClose);
		}

		TEST_F(ServerTest, SetIPPortHandler) {
			EXPECT_CALL(socket_calls_mock, NewSocket(AF_INET, _, _)).Times(2).WillRepeatedly(Return(-1));

			EXPECT_EQ(server_->setIpAddress(0x0200007F), 0x0200007F);
			EXPECT_EQ(server_->getStatus(), Server::Status::kErrSocketInit);
			EXPECT_EQ(server_->setPort(4000), 4000);
			EXPECT_EQ(server_->getStatus(), Server::Status::kErrSocketInit);
			EXPECT_EQ(server_->getIpAddress(), 0x0200007F);
			EXPECT_EQ(server_->getPort(), 4000);

			EXPECT_NO_THROW(server_->setHandler(handler));
		}

		TEST_F(ServerTest, CorrectStart) {
			EXPECT_CALL(socket_calls_mock, NewSocket(AF_INET, _, _)).Times(2).WillRepeatedly(Return(100));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(100, _, _, _, _)).Times(4).WillRepeatedly(Return(0));
#ifdef _WIN32
			EXPECT_CALL(socket_calls_mock, IOctlSocket(100, _, _)).Times(2).WillRepeatedly(Return(0));
#endif
			EXPECT_CALL(socket_calls_mock, Bind(100, _)).Times(2).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, Listen(100, SOMAXCONN)).Times(2).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, Close(100)).WillRepeatedly(Return(0));

			EXPECT_EQ(server_->start(), Server::Status::kUp);
			EXPECT_EQ(server_->start(), Server::Status::kUp);
		}

#ifdef _WIN32

		TEST_F(ServerTest, errSocketInitIOctlSocket) {
			EXPECT_CALL(socket_calls_mock, NewSocket(AF_INET, _, _)).Times(2).WillRepeatedly(Return(100));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(100, _, _, _, _)).Times(4).WillRepeatedly(Return(0));

			EXPECT_CALL(socket_calls_mock, IOctlSocket(100, _, _)).Times(2).WillRepeatedly(Return(-1));

			EXPECT_EQ(server_->start(), Server::Status::kErrSocketInit);
		}

#endif

		TEST_F(ServerTest, errSocket) {
			EXPECT_CALL(socket_calls_mock, NewSocket(AF_INET, _, _)).Times(2).WillRepeatedly(Return(100));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(100, _, _, _, _)).Times(4).WillRepeatedly(Return(0));
#ifdef _WIN32
			EXPECT_CALL(socket_calls_mock, IOctlSocket(100, _, _)).Times(2).WillRepeatedly(Return(0));
#endif
			EXPECT_CALL(socket_calls_mock, Bind(100, _)).Times(2).WillOnce(Return(-1)).WillOnce(Return(0));
			EXPECT_CALL(socket_calls_mock, Listen(100, SOMAXCONN)).Times(1).WillOnce(Return(-1));

			EXPECT_EQ(server_->start(), Server::Status::kErrSocketBind);
			EXPECT_EQ(server_->start(), Server::Status::kErrSocketListening);
		}
	}
}