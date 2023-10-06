#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "socketCallsMock.hpp"

#include "../ApplicationClient.hpp"
#include "../ApplicationClient.cpp"

using ::testing::Return;
using ::testing::Throw;
using ::testing::_;
using ::testing::ElementsAre;

using namespace SocketTCP;

namespace ApplicationClient {
	
	static SocketCallsMock socket_calls_mock_ = SocketCallsMock();
	
	class HandleTest : public ::testing::Test {
	public:
		Client::Client* client_ = nullptr;		

		void SetUp() {
			client_ = new Client::Client();
			client_->sockets = &socket_calls_mock_;
		}
		void TearDown() { delete client_; }
	};

	TEST_F(HandleTest, StandardUsing) {
		DataBuffer data = {'a', 'b', 'c', 'd'};
		std::istringstream input("set a\nexit\n");

		Socket actualSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		EXPECT_CALL(socket_calls_mock_, NewSocket(AF_INET, SOCK_STREAM, IPPROTO_IP))
			.Times(1).WillOnce(Return(actualSocket));
		EXPECT_CALL(socket_calls_mock_, SetSockOptions(actualSocket, SOL_SOCKET, SO_RCVTIMEO, _, _))
			.Times(1).WillOnce(Return(0));
		EXPECT_CALL(socket_calls_mock_, Connect(actualSocket, _)).Times(1).WillOnce(Return(0));

		EXPECT_CALL(socket_calls_mock_, Send(actualSocket, ElementsAre(6, 0, 0, 0, 0, 0, 0, 0, '$', 'd', 's', 'c', 'n', 0), _))
			.Times(1).WillOnce(Return(0));
		EXPECT_CALL(socket_calls_mock_, Send(actualSocket, ElementsAre(6, 0, 0, 0, 0, 0, 0, 0, 's', 'e', 't', ' ', 'a', 0), _))
			.Times(1).WillOnce(Throw(std::runtime_error(""))).RetiresOnSaturation();
		
		EXPECT_CALL(socket_calls_mock_, Shutdown(actualSocket, SD_BOTH)).Times(1).WillOnce(Return(0));
		EXPECT_CALL(socket_calls_mock_, Close(actualSocket)).Times(1).WillOnce(Return(0));

		EXPECT_EQ(client_->connectTo(0x0100007F, 1000), ClientSocketStatus::kConnected);
		
		ASSERT_NO_THROW(HandleClient(*client_, true, data, input));
		ASSERT_NO_THROW(HandleClient(*client_, true, data, input));

		if (actualSocket != WIN(INVALID_SOCKET)NIX(-1))
			WIN(closesocket)NIX(close)(actualSocket);
	}

	TEST(IsConsole, CheckStandard) {
		ASSERT_NO_THROW(IsConsole());
	}

}