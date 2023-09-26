#pragma once
#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include "Shared.hpp"
#include <functional>
#include <condition_variable>

class ThreadPool {
	std::vector<std::jthread> _threads;
	std::queue<std::function<void()>> _jobs;
	mutex _jobsMutex;
	std::condition_variable _condition;
	std::atomic<bool> _threadsTerminated = false;

	void setupThreadPool(u64 threadCount);
	void workerLoop();

public:
	ThreadPool(u64 threadCount = std::thread::hardware_concurrency());
	~ThreadPool();

	template<typename F>
	void addJob(F job)
	{
		if (_threadsTerminated) return;

		std::unique_lock lock(_jobsMutex);
		_jobs.push(std::function<void()>(job));
		_condition.notify_one();
	}

	template<typename F, typename... Arg>
	void addJob(const F& job, const Arg&... args)
	{
		addJob([job, args...] {job(args...); });
	}

	void join();

	u64 getThreadCount() const;
	void dropUnstartedJobs();

	void stop();
	void start(u64 threadCount = std::thread::hardware_concurrency());

};

#endif // !THREADPOOL_HPP
