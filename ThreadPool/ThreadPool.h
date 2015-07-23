#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#pragma once

#ifdef THREADPOOL_EXPORTS
#define THREADPOOLDLL_API __declspec(dllexport) 
#else
#define THREADPOOLDLL_API __declspec(dllimport) 
#endif

#include <thread>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <functional>
#include <vector>
#include <atomic>

namespace THREADING {
	class Worker {
	public:
		Worker(ThreadPool &);
		void operator()();
	private:
		ThreadPool &pool;
	};

	class ThreadPool {
	public:
		THREADPOOLDLL_API ThreadPool(size_t);
		THREADPOOLDLL_API ~ThreadPool();
		template<class T>
		THREADPOOLDLL_API void enqueue(T);
	private:
		friend class Worker;
		std::vector<std::thread> workers;
		std::deque<std::function<void()>> taskQueue;
		std::mutex queueMutex;
		std::condition_variable cv;
		std::atomic<bool> stop;
	};
}

#endif

