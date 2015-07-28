#include "ThreadPool.h"
#include "Filewriter.h"
#include "Filesystem.h"
#include "Testing.h"

#include <string>
#include <iostream>
#include <array>
#include <thread>

static bool failure = false;
static std::string data[numWriters];

inline void startWriter(STORAGE::Filesystem *fs, int ind) {
	File &f = fs->select("TestFile");
	STORAGE::IO::SafeWriter writer = fs->getSafeWriter(f);
	writer.write(data[ind].c_str(), data[ind].size());
}

inline bool startReader(STORAGE::Filesystem *fs) {
	File &f = fs->select("TestFile");
	STORAGE::IO::SafeReader reader = fs->getSafeReader(f);
	std::string res;
	bool valid = true;	
	res = reader.readString();
	for (int ind = 0; ind < numWriters; ++ind) {
		if (res.compare(data[ind]) == 0 || res.compare("") == 0) {
			valid = true;
			break;
		}
	}
	return valid;
}

int TestConcurrentReadWrite(STORAGE::Filesystem *fs) {

	for (int i = 0; i < numWriters; ++i) {
		std::srand((unsigned int)std::time(NULL) + i);
		data[i] = random_string(dataSize);
	}

	THREADING::ThreadPool pool(numThreads);

	std::thread writeThread([&pool, fs] {
		for (int i = 0; i < numWriters; ++i) {
			pool.enqueue([fs, i] {startWriter(fs, i); });
		}
	});

	std::thread readThread([&pool, fs] {
		for (int i = 0; i < numReaders; ++i) {
			std::future<bool> ret = pool.enqueue([fs] {return startReader(fs); });
			if (!ret.get()) {
				failure = true;
				break;
			}
		}
	});

	readThread.join();
	writeThread.join();

	return failure;
}