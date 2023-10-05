#include "ThreadPool.hpp"

void ThreadPool::ThreadPool::setupThreadPool(u64 thread_count)
{
	threads_.clear();
	for (u64 i = 0; i < thread_count; ++i)
		threads_.push_back(std::jthread([&]() { workerLoop(); }));
}

void ThreadPool::ThreadPool::workerLoop()
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

ThreadPool::ThreadPool::ThreadPool(u64 threadCount)
{
	setupThreadPool(threadCount);
}

ThreadPool::ThreadPool::~ThreadPool()
{
	halt();
	join();
}

void ThreadPool::ThreadPool::join()
{
	for (auto& thread : threads_)
		if (thread.joinable())
		{
			condition_.notify_all();
			thread.join();
		}
	threads_.clear();
}

u64 ThreadPool::ThreadPool::getThreadCount() const
{
	return threads_.size();
}

void ThreadPool::ThreadPool::dropUnstartedJobs()
{
	halt();
	join();
	std::queue<std::function<void()>> empty;
	std::swap(jobs_, empty);
	setupThreadPool(threads_.size());
}

void ThreadPool::ThreadPool::halt()
{
	halt_ = true;
	condition_.notify_all();
	for (auto& thread : threads_)
		thread.request_stop();
}

void ThreadPool::ThreadPool::start(u64 threadCount)
{
	setupThreadPool(threadCount);
}