#include "ThreadPool.h"

/*
 *  Worker class
 */

THREADING::Worker::Worker(ThreadPool &pool_) : pool(pool_) { }

void THREADING::Worker::operator()() {
	std::function<void()> task;
	while (true) {
		std::unique_lock<std::mutex> lk(pool.queueMutex);
		{
			pool.cv.wait(lk, [&] {return pool.stop.load() || !pool.taskQueue.empty(); });
			if (pool.stop.load()) {
				return;
			}
			task = pool.taskQueue.front();
			pool.taskQueue.pop_front();
		}
		lk.unlock();
		task();
	}
}

 /*
 *  ThreadPool class
 */

THREADING::ThreadPool::ThreadPool(size_t n) {
	// Create some threads
	stop.store(false);
	for (size_t i = 0; i < n; ++i) {
		workers.push_back(std::thread(Worker(*this)));
	}
}

template <class T>
void THREADING::ThreadPool::enqueue(T f) {
	// Add worker to queue
	std::unique_lock<std::mutex> lk(queueMutex);
	{
		taskQueue.push_back(f);
	}
	lk.unlock();
	cv.notify_one();
}

THREADING::ThreadPool::~ThreadPool() {
	// Clean up
	stop.store(true);
	cv.notify_all();
	for (size_t i = 0; i < workers.size(); ++i) {
		workers[i].join();
	}
}