#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "../RequestState.hpp"

namespace Database {

	TEST(RequestStateObject, Constructors) {
		RequestStateObject a = RequestStateObject();
		ASSERT_EQ(a.message_, "");
		ASSERT_EQ(a.state_, RequestState::kNone);
		RequestStateObject b = RequestStateObject("something", RequestState::kWarning);
		ASSERT_EQ(b.message_, "something");
		ASSERT_EQ(b.state_, RequestState::kWarning);
	}

	TEST(RequestStateObject, ColorizedMessage) {
		RequestStateObject a = RequestStateObject("something", RequestState::kNone);
		ASSERT_EQ(a.ColorizedMessage(), "something");
		a.state_ = RequestState::kOk;
		ASSERT_EQ(a.ColorizedMessage(), "\033[92msomething\033[0m");
		a.state_ = RequestState::kWarning;
		ASSERT_EQ(a.ColorizedMessage(), "\033[93msomething\033[0m");
		a.state_ = RequestState::kError;
		ASSERT_EQ(a.ColorizedMessage(), "\033[91msomething\033[0m");
		a.state_ = RequestState::kInfo;
		ASSERT_EQ(a.ColorizedMessage(), "\033[96msomething\033[0m");
	}

}