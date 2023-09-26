#include "ThreadPool.hpp"

void ThreadPool::setupThreadPool(u64 threadCount)
{
	_threads.clear();
	for (u64 i = 0; i < threadCount; ++i)
		_threads.emplace_back(&ThreadPool::workerLoop, this);
}

void ThreadPool::workerLoop()
{
	std::function<void()> job;
	while (!_threadsTerminated) {
		{
			std::unique_lock lock(_jobsMutex);
			_condition.wait(lock, [this]()
				{
					return !_jobs.empty() || _threadsTerminated;
				});
			if (_threadsTerminated) return;
			job = _jobs.front();
			_jobs.pop();
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
	_threadsTerminated = true;
	join();
}

void ThreadPool::join()
{
	for (auto& thread : _threads)
	{
		_condition.notify_all();
		thread.join();
	}
}

u64 ThreadPool::getThreadCount() const
{
	return _threads.size();
}

void ThreadPool::dropUnstartedJobs()
{
	_threadsTerminated = true;
	join();
	std::queue<std::function<void()>> empty;
	std::swap(_jobs, empty);
	_threadsTerminated = false;
	setupThreadPool(_threads.size());
}

void ThreadPool::stop()
{
	_threadsTerminated = true;
	join();
}

void ThreadPool::start(u64 threadCount)
{
	if (!_threadsTerminated) return;
	_threadsTerminated = false;
	setupThreadPool(threadCount);
}