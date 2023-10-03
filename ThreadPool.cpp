#include "ThreadPool.hpp"

void ThreadPool::setupThreadPool(u64 thread_count)
{
	thread.clear();
	for (u64 i = 0; i < thread_count; ++i)
		thread.push_back(std::jthread([&]() { workerLoop(); }));
}

void ThreadPool::workerLoop()
{
	std::function<void()> job;
	while (not halt_) {
		{
			std::unique_lock lock(job_mutex_);
			condition_.wait(lock, [this]()
				{
					return not jobs_.empty() or halt_;
				});
			if (halt_) return;
			job = jobs_.front();
			jobs_.pop();
		}
		job();
	}
}

ThreadPool::ThreadPool(u64 threadCount)
{
	setupThreadPool(threadCount);
}

ThreadPool::~ThreadPool()
{
	halt();
}

void ThreadPool::join()
{
	for (auto& thread : thread)
		if (thread.joinable())
		{
			condition_.notify_all();
			thread.join();
		}
}

u64 ThreadPool::getThreadCount() const
{
	return thread.size();
}

void ThreadPool::dropUnstartedJobs()
{
	join();
	std::queue<std::function<void()>> empty;
	std::swap(jobs_, empty);
	setupThreadPool(thread.size());
}

void ThreadPool::halt()
{
	halt_ = true;
	condition_.notify_all();
	for (auto& thread : thread)
		thread.request_stop();
}

void ThreadPool::start(u64 threadCount)
{
	setupThreadPool(threadCount);
}