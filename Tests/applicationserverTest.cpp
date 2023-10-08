#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "socketCallsMock.hpp"

#include "../ApplicationServer.hpp"
#include "../ApplicationServer.cpp"

using ::testing::Return;
using ::testing::Throw;
using ::testing::_;
using ::testing::ElementsAre;

namespace ApplicationServerTest {

	class ApplicationServerTest : public ::testing::Test {
	public:
		ApplicationServer* app = nullptr;

		void SetUp() { app = new ApplicationServer(); }
		void TearDown() { delete app; }
	};

	TEST_F(ApplicationServerTest, DeathTest) {
		std::istringstream input("help\nstate\nset a\ntree\nsomething\nexit\n");
		ASSERT_NO_FATAL_FAILURE(app->Run(input));
	}
}