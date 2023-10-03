#pragma once
#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include "Shared.hpp"
#include <functional>
#include <condition_variable>

class ThreadPool {
	std::atomic_bool halt_ = false;
	std::vector<std::jthread> thread;
	std::queue<std::function<void()>> jobs_;
	mutex job_mutex_;
	std::condition_variable condition_;

	void setupThreadPool(u64 thread_count);
	void workerLoop();

public:
	ThreadPool(u64 thread_count = std::thread::hardware_concurrency());
	~ThreadPool();

	template<typename F>
	void addJob(F job)
	{
		if (halt_) return;
		std::unique_lock lock(job_mutex_);
		jobs_.push(std::function<void()>(job));
		condition_.notify_one();
	}

	template<typename F, typename... Arg>
	void addJob(const F& job, const Arg&... args)
	{
		addJob([job, args...] {job(args...); });
	}

	void join();

	u64 getThreadCount() const;
	void dropUnstartedJobs();

	void halt();
	void start(u64 thread_count = std::thread::hardware_concurrency());

};

#endif // !THREADPOOL_HPP
