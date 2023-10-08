#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "../Object.hpp"
#include "../Object.cpp"

using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::SaveArg;
using ::testing::Return;
using ::testing::ElementsAre;
using ::testing::_;

namespace Database {

	class ObjectTest : public ::testing::Test {
	public:
		std::shared_ptr<Object> root_ = nullptr;

		void SetUp() { root_.reset(new Object("root")); }
		void TearDown() { root_.reset(); }
	};

	TEST_F(ObjectTest, HandleStandardOperators) {
		ASSERT_NO_THROW(root_->SetThisPtr(root_));

		ASSERT_EQ(root_->Set("a").state_, RequestState::kOk);
		ASSERT_EQ(root_->getChildren().size(), 1);
		ASSERT_EQ(root_->getChildren()["a"]->getName(), "a");

		ASSERT_EQ(root_->Set("").state_, RequestState::kError);
		ASSERT_EQ(root_->getChildren().size(), 1);

		ASSERT_EQ(root_->Set("b,c").state_, RequestState::kError);
		ASSERT_EQ(root_->getChildren().size(), 1);

		ASSERT_EQ(root_->Set("a").state_, RequestState::kWarning);
		ASSERT_EQ(root_->getChildren().size(), 1);

		ASSERT_EQ(root_->Get("").state_, RequestState::kOk);
		ASSERT_EQ(root_->Get("").message_, "Objects: 1\n[\n   a,\n]\n");

		ASSERT_EQ(root_->Get("b").state_, RequestState::kError);

		ASSERT_EQ(root_->Get("a").state_, RequestState::kOk);
		ASSERT_EQ(root_->Get("a").message_, "Objects: 0\n[]\n");

		ASSERT_EQ(root_->Tree("").state_, RequestState::kOk);
		ASSERT_EQ(root_->Tree("b").state_, RequestState::kError);
		ASSERT_EQ(root_->Tree("a").state_, RequestState::kOk);

		ASSERT_EQ(root_->Export("").state_, RequestState::kOk);
		ASSERT_EQ(root_->Export("b").state_, RequestState::kError);
		ASSERT_EQ(root_->Export("a").state_, RequestState::kOk);

		ASSERT_EQ(root_->Check("").state_, RequestState::kError);
		ASSERT_EQ(root_->Check("b").state_, RequestState::kWarning);
		ASSERT_EQ(root_->Check("a").state_, RequestState::kOk);

		ASSERT_EQ(root_->Set("b").state_, RequestState::kOk);
		ASSERT_EQ(root_->getChildren().size(), 2);
		ASSERT_EQ(root_->getChildren()["b"]->getName(), "b");

		ASSERT_EQ(root_->Remove("").state_, RequestState::kError);
		ASSERT_EQ(root_->Remove("c").state_, RequestState::kError);
		ASSERT_EQ(root_->Remove("b").state_, RequestState::kOk);
		ASSERT_EQ(root_->getChildren().size(), 1);
		ASSERT_EQ(root_->getChildren().contains("b"), false);

		ASSERT_EQ(root_->Clear("b").state_, RequestState::kError);
		ASSERT_EQ(root_->Clear("a").state_, RequestState::kOk);
		root_->anchors_.fetch_add(1);
		ASSERT_EQ(root_->Clear("").state_, RequestState::kOk);
		ASSERT_EQ(root_->getChildren().size(), 0);
	}

	TEST_F(ObjectTest, HandleSeparator) {
		auto& func = get<0>(kOperations.at("set"));
		std::vector<string> objects = {"a", "b", "c"};
		std::vector<string> objects2 = {};
		ASSERT_EQ(root_->HandleComma(func, objects).state_, RequestState::kOk);
		ASSERT_EQ(root_->HandleComma(func, objects2).state_, RequestState::kError);
		ASSERT_EQ(root_->getChildren().size(), 3);
		ASSERT_EQ(root_->getChildren().contains("a"), true);
		ASSERT_EQ(root_->getChildren().contains("b"), true);
		ASSERT_EQ(root_->getChildren().contains("c"), true);
		std::vector<string> b = {"a", "d"};
		std::vector<string> e = {"e", "g"};
		ASSERT_EQ(root_->HandleSelector(func, b).state_, RequestState::kOk);
		ASSERT_EQ(root_->HandleSelector(func, e).state_, RequestState::kError);
		ASSERT_EQ(root_->getChildren().size(), 3);
		ASSERT_EQ(root_->getChildren().contains("a"), true);
		ASSERT_EQ(root_->getChildren()["a"]->getChildren().size(), 1);
		ASSERT_EQ(root_->getChildren()["a"]->getChildren().contains("d"), true);
	}

	TEST_F(ObjectTest, HandleRequest) {
		string cmd = "noo";
		ASSERT_EQ(root_->HandleRequest(cmd).state_, RequestState::kWarning);
		cmd = "?";
		ASSERT_EQ(root_->HandleRequest(cmd).state_, RequestState::kInfo);
		cmd = "tree";
		ASSERT_EQ(root_->HandleRequest(cmd).state_, RequestState::kOk);
		cmd = "noo a";
		ASSERT_EQ(root_->HandleRequest(cmd).state_, RequestState::kWarning);
		cmd = "set a";
		ASSERT_EQ(root_->HandleRequest(cmd).state_, RequestState::kOk);
	}
}