#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "../ThreadPool.hpp"
#include "../ThreadPool.cpp"

namespace ThreadPool {

	using namespace std::chrono_literals;

	class ThreadPoolTest : public ::testing::Test {
	public:
		ThreadPool* pool_ = nullptr;
		const u64 kThreadsAmount = 4;
		const u64 kWaitingTime = 200; // ms

		void SetUp() { pool_ = new ThreadPool(kThreadsAmount); }
		void TearDown() { delete pool_; }
	};

	TEST_F(ThreadPoolTest, CheckGetThreadCount) {
		ASSERT_EQ(pool_->getThreadCount(), kThreadsAmount);
	}

	TEST_F(ThreadPoolTest, CheckJobAdding) {
		u64 test_value = 0;
		pool_->addJob([&test_value]() { test_value = -1; });

		auto start = std::chrono::high_resolution_clock::now();

		while (test_value != -1 and
			(std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start)).count() < kWaitingTime) {}

		ASSERT_EQ(test_value, -1);
	}

	TEST_F(ThreadPoolTest, CheckJoin) {
		pool_->halt();
		pool_->join();
		auto start = std::chrono::high_resolution_clock::now();
		while (pool_->getThreadCount() != 0 and
			(std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start)).count() < kWaitingTime) {}

		ASSERT_EQ(pool_->getThreadCount(), 0);
	}

	TEST_F(ThreadPoolTest, CheckDropUnstartedJobs) {
		pool_->halt();
		pool_->join();
		std::atomic<u64> test_value = 0;
		for (u64 i = 0; i < 1000; ++i)
			pool_->addJob([&test_value]() { std::this_thread::sleep_for(5ms); test_value.fetch_add(1); });
		pool_->start(kThreadsAmount);
		pool_->dropUnstartedJobs();
		pool_->halt();
		pool_->join();
		ASSERT_LE(test_value, 500);
	}
}