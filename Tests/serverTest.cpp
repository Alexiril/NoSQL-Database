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

		using namespace std::chrono_literals;

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
			EXPECT_EQ(server_->explainStatus(), "Socket is closed.");
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
			EXPECT_CALL(socket_calls_mock, NewSocket(AF_INET, _, _)).Times(2).WillRepeatedly(Return(1));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, _, _, _, _)).Times(4).WillRepeatedly(Return(0));
#ifdef _WIN32
			EXPECT_CALL(socket_calls_mock, IOctlSocket(1, _, _)).Times(2).WillRepeatedly(Return(0));
#endif
			EXPECT_CALL(socket_calls_mock, Bind(1, _)).Times(2).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, Listen(1, SOMAXCONN)).Times(2).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, Close(1)).WillRepeatedly(Return(0));

			EXPECT_EQ(server_->start(), Server::Status::kUp);
			EXPECT_EQ(server_->start(), Server::Status::kUp);
			EXPECT_EQ(server_->explainStatus(), "Socket is up.");
			server_->joinLoop();
		}

#ifdef _WIN32

		TEST_F(ServerTest, ErrSocketInitIOctlSocket) {
			EXPECT_CALL(socket_calls_mock, NewSocket(AF_INET, _, _)).Times(2).WillRepeatedly(Return(1));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, _, _, _, _)).Times(4).WillRepeatedly(Return(0));

			EXPECT_CALL(socket_calls_mock, IOctlSocket(1, _, _)).Times(2).WillRepeatedly(Return(-1));

			EXPECT_EQ(server_->start(), Server::Status::kErrSocketInit);
			WSASetLastError(1);
			EXPECT_EQ(server_->explainStatus(), "Error: couldn't initialize a socket (1).");
		}

		TEST_F(ServerTest, EnableKeepAlive) {
			EXPECT_CALL(socket_calls_mock, SetSockOptions(0, SOL_SOCKET, SO_KEEPALIVE, _, _))
				.Times(3).WillOnce(Return(0)).WillOnce(Return(0)).WillOnce(Return(-1));
			EXPECT_CALL(socket_calls_mock, wsaioctl(0, SIO_KEEPALIVE_VALS, _, _, nullptr, 0, _, 0, nullptr))
				.Times(2).WillOnce(Return(0)).WillOnce(Return(-1));

			EXPECT_EQ(server_->enableKeepAlive(0), true);
			EXPECT_EQ(server_->enableKeepAlive(0), false);
			EXPECT_EQ(server_->enableKeepAlive(0), false);
		}

#else

		TEST_F(ServerTest, EnableKeepAlive) {
			EXPECT_CALL(socket_calls_mock, SetSockOptions(0, SOL_SOCKET, SO_KEEPALIVE, _, _))
				.Times(5)
				.WillOnce(Return(0))
				.WillOnce(Return(0))
				.WillOnce(Return(0))
				.WillOnce(Return(0))
				.WillOnce(Return(-1));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(0, IPPROTO_TCP, TCP_KEEPIDLE, _, _))
				.Times(4)
				.WillOnce(Return(0))
				.WillOnce(Return(0))
				.WillOnce(Return(0))
				.WillOnce(Return(-1));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(0, IPPROTO_TCP, TCP_KEEPINTVL, _, _))
				.Times(3)
				.WillOnce(Return(0))
				.WillOnce(Return(0))
				.WillOnce(Return(-1));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(0, IPPROTO_TCP, TCP_KEEPCNT, _, _))
				.Times(2)
				.WillOnce(Return(0))
				.WillOnce(Return(-1));

			EXPECT_EQ(server_->enableKeepAlive(0), true);
			EXPECT_EQ(server_->enableKeepAlive(0), false);
			EXPECT_EQ(server_->enableKeepAlive(0), false);
			EXPECT_EQ(server_->enableKeepAlive(0), false);
			EXPECT_EQ(server_->enableKeepAlive(0), false);
		}

#endif

		TEST_F(ServerTest, ErrSocket) {
			EXPECT_CALL(socket_calls_mock, NewSocket(AF_INET, _, _)).Times(2).WillRepeatedly(Return(1));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, _, _, _, _)).Times(4).WillRepeatedly(Return(0));
#ifdef _WIN32
			EXPECT_CALL(socket_calls_mock, IOctlSocket(1, _, _)).Times(2).WillRepeatedly(Return(0));
#endif
			EXPECT_CALL(socket_calls_mock, Bind(1, _)).Times(2).WillOnce(Return(-1)).WillOnce(Return(0));
			EXPECT_CALL(socket_calls_mock, Listen(1, SOMAXCONN)).Times(1).WillOnce(Return(-1));

			EXPECT_EQ(server_->start(), Server::Status::kErrSocketBind);
			WSASetLastError(1);
			EXPECT_EQ(server_->explainStatus(), "Error: couldn't bind a socket (1).");
			EXPECT_EQ(server_->start(), Server::Status::kErrSocketListening);
			WSASetLastError(1);
			EXPECT_EQ(server_->explainStatus(), "Error: couldn't listen a socket (1).");
		}

		TEST_F(ServerTest, ClientHandling) {
			EXPECT_CALL(socket_calls_mock, FdSet(0, _)).Times(1);
			EXPECT_CALL(socket_calls_mock, SelectBeforeAccept(0, _, _)).Times(1);
			EXPECT_CALL(socket_calls_mock, FdIsSet(0, _)).Times(1).WillOnce(Return(true));
			EXPECT_CALL(socket_calls_mock, Accept(0, _)).Times(1).WillOnce(Return(1));

#ifdef _WIN32
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, SOL_SOCKET, SO_KEEPALIVE, _, _)).WillOnce(Return(0));
			EXPECT_CALL(socket_calls_mock, wsaioctl(1, SIO_KEEPALIVE_VALS, _, _, nullptr, 0, _, 0, nullptr)).WillOnce(Return(0));
#else
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, SOL_SOCKET, SO_KEEPALIVE, _, _)).WillOnce(Return(0));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, IPPROTO_TCP, TCP_KEEPIDLE, _, _)).WillOnce(Return(0));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, IPPROTO_TCP, TCP_KEEPINTVL, _, _)).WillOnce(Return(0));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, IPPROTO_TCP, TCP_KEEPCNT, _, _)).WillOnce(Return(0));
#endif
			EXPECT_CALL(socket_calls_mock, Recv(1, _, _, _))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 2, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 'a', 0}), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 2, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 'a', 0 }), Return(RecvResult::kOk)))
				.WillOnce(Return(RecvResult::kDisconnect));

			EXPECT_NO_THROW(server_->halt());
			EXPECT_NO_THROW(server_->handlingAcceptLoop());
			EXPECT_NO_THROW(server_->waitingDataLoop());
			EXPECT_NO_THROW(server_->waitingDataLoop());
			EXPECT_NO_THROW(server_->waitingDataLoop());
		}

		TEST_F(ServerTest, ClientHandlingNoHalt) {
			EXPECT_CALL(socket_calls_mock, FdSet(0, _)).WillRepeatedly(Return());
			EXPECT_CALL(socket_calls_mock, SelectBeforeAccept(0, _, _)).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, FdIsSet(0, _)).WillRepeatedly(Return(true));
			EXPECT_CALL(socket_calls_mock, Accept(0, _)).WillRepeatedly(Return(1));

#ifdef _WIN32
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, SOL_SOCKET, SO_KEEPALIVE, _, _)).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, wsaioctl(1, SIO_KEEPALIVE_VALS, _, _, nullptr, 0, _, 0, nullptr)).WillRepeatedly(Return(0));
#else
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, SOL_SOCKET, SO_KEEPALIVE, _, _)).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, IPPROTO_TCP, TCP_KEEPIDLE, _, _)).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, IPPROTO_TCP, TCP_KEEPINTVL, _, _)).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, IPPROTO_TCP, TCP_KEEPCNT, _, _)).WillRepeatedly(Return(0));
#endif
			EXPECT_CALL(socket_calls_mock, Recv(1, _, _, _))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 2, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 'a', 0 }), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 2, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 'a', 0 }), Return(RecvResult::kOk)))
				.WillRepeatedly(Return(RecvResult::kDisconnect));

			EXPECT_NO_THROW(server_->handlingAcceptLoop());
			EXPECT_NO_THROW(server_->waitingDataLoop());
			std::this_thread::sleep_for(20ms);
			server_->halt();
			server_->joinLoop();
		}

		TEST_F(ServerTest, HandleAcceptNoKeepAlive) {
			EXPECT_CALL(socket_calls_mock, FdSet(0, _)).WillRepeatedly(Return());
			EXPECT_CALL(socket_calls_mock, SelectBeforeAccept(0, _, _)).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, FdIsSet(0, _)).WillRepeatedly(Return(true));
			EXPECT_CALL(socket_calls_mock, Accept(0, _)).WillRepeatedly(Return(1));
#ifdef _WIN32
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, SOL_SOCKET, SO_KEEPALIVE, _, _)).WillRepeatedly(Return(-1));
#else
			EXPECT_CALL(socket_calls_mock, SetSockOptions(1, SOL_SOCKET, SO_KEEPALIVE, _, _)).WillRepeatedly(Return(-1));
#endif
			EXPECT_CALL(socket_calls_mock, Shutdown(1, 0)).WillRepeatedly(Return(0));
			EXPECT_CALL(socket_calls_mock, Close(1)).WillRepeatedly(Return(0));

			EXPECT_NO_THROW(server_->handlingAcceptLoop());
			EXPECT_NO_THROW(server_->waitingDataLoop());
		}

		TEST(ServerClient, CheckDestructor) {
			Server::Client* client = new Server::Client(2);
			EXPECT_NO_THROW(delete client);
		}

		class ServerClientTest : public ::testing::Test {
		public:
			Server::Client* client_ = nullptr;
			const u64 kWaitingTime = 200; // ms

			void SetUp() { 
				client_ = new Server::Client(1);
				client_->sockets = &socket_calls_mock;
			}
			void TearDown() { delete client_; }
		};

		TEST_F(ServerClientTest, CorrectProcess) {

			DataBuffer sendBuffer = DataBuffer(256);

			EXPECT_CALL(socket_calls_mock, Send(1, _, _)).Times(1).WillOnce(Return(-1));
			EXPECT_CALL(socket_calls_mock, Send(1, ElementsAre(4, 0, 0, 0, 0, 0, 0, 0, 'a', 'b', 'c', 0), _))
				.Times(1).WillOnce(Return(0)).RetiresOnSaturation();
			EXPECT_CALL(socket_calls_mock, Recv(1, _, _, _))
				.Times(3).WillOnce(Return(RecvResult::kOk)).WillOnce(Return(RecvResult::kAgain)).WillOnce(Return(RecvResult::kDisconnect));
			EXPECT_CALL(socket_calls_mock, Shutdown(1, SD_BOTH)).Times(1).WillOnce(Return(0));
			EXPECT_CALL(socket_calls_mock, Close(1)).Times(1).WillOnce(Return(0));

			EXPECT_EQ(client_->sendData("abc"), true);
			EXPECT_EQ(client_->sendData("def"), false);
			EXPECT_EQ(client_->loadData().size(), 0);
			EXPECT_EQ(client_->loadData().size(), 0);
			EXPECT_EQ(client_->loadData().size(), 0);

		}

		TEST_F(ServerClientTest, LoadData) {
			EXPECT_CALL(socket_calls_mock, Recv(1, _, _, _))
				.Times(6)
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 10, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }), Return(RecvResult::kOk)))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 10, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(Return(RecvResult::kAgain))
				.WillOnce(DoAll(SetArgPointee<1>(DataBuffer{ 10, 0, 0, 0 }), Return(RecvResult::kOk)))
				.WillOnce(Return(RecvResult::kDisconnect));
			EXPECT_CALL(socket_calls_mock, Shutdown(1, SD_BOTH)).Times(1).WillOnce(Return(0));
			EXPECT_CALL(socket_calls_mock, Close(1)).Times(1).WillOnce(Return(0));

			EXPECT_THAT(client_->loadData(), ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
			EXPECT_EQ(client_->loadData().size(), 0);
			EXPECT_EQ(client_->loadData().size(), 0);
		}
	}
}