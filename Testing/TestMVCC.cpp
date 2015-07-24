#include "ThreadPool.h"
#include "Filewriter.h"
#include "Filesystem.h"
#include "Testing.h"

#include <string>
#include <iostream>
#include <array>
#include <thread>

const int numThreads = 32;
const int numWriters = 128;
const int numReaders = 16;
static bool failure = false;
static std::string data[numWriters];

void startWriter(STORAGE::Filesystem *fs, int ind) {
	File f = fs->select("TestFile");
	STORAGE::IO::Writer writer = fs->getWriter(f);
	fs->lock(f, STORAGE::IO::EXCLUSIVE);
	{
		writer.write(data[ind].c_str(), data[ind].size());
	}
	fs->unlock(f, STORAGE::IO::EXCLUSIVE);
}

bool startReader(STORAGE::Filesystem *fs) {
	File f = fs->select("TestFile");
	STORAGE::IO::Reader reader = fs->getReader(f);
	std::string res;
	bool valid = false;
	fs->lock(f, STORAGE::IO::NONEXCLUSIVE);
	{
		res = reader.readString();
	}
	fs->unlock(f, STORAGE::IO::NONEXCLUSIVE);
	for (int ind = 0; ind < numWriters; ++ind) {
		if (res.compare(data[ind]) == 0) {
			valid = true;
			break;
		}
	}
	return valid;
}

int TestMVCC(STORAGE::Filesystem *fs) {

	for (int i = 0; i < numWriters; ++i) {
		std::srand((unsigned int)std::time(NULL) + i);
		data[i] = random_string(16);
	}

	THREADING::ThreadPool pool(numThreads);

	fs->toggleMVCC();

	for (int i = 0; i < numWriters; ++i) {
		pool.enqueue([fs, i] {startWriter(fs, i); });
	}

	for (int i = 0; i < numReaders; ++i) {
		std::future<bool> ret = pool.enqueue([&] {return startReader(fs); });
		if (!ret.get()) {
			failure = true;
			break;
		}
	}

	fs->toggleMVCC();

	return failure;
}