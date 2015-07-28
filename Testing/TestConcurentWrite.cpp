#include "ThreadPool.h"

#include "Filewriter.h"
#include "Filesystem.h"
#include "Testing.h"

#include <string>
#include <iostream>
#include <array>
#include <thread>

static bool failure = false;

int TestConcurrentWrite(STORAGE::Filesystem *fs) {
	THREADING::ThreadPool pool(numThreads);

	for (int i = 0; i < numWriters; ++i) {
		std::future<int> ret = pool.enqueue([&] () {return TestReadWrite(fs); });
		if (ret.get() != 0) {
			failure = true;
			break;
		}
	}
	return failure;
}