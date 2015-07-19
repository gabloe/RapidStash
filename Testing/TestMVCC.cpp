#include "Filewriter.h"
#include "Filesystem.h"
#include "Testing.h"

#include <string>
#include <iostream>
#include <array>
#include <thread>

const int numThreads = 32;

static bool failure = false;

static std::string data[numThreads];

int TestMVCC(STORAGE::Filesystem *fs) {

	for (int i = 0; i < numThreads; ++i) {
		std::srand((unsigned int)std::time(NULL) + i);
		data[i] = random_string(16);
	}

	fs->toggleMVCC();

	std::array<std::thread, numThreads> writers;
	std::array<std::thread, numThreads> readers;

	// Start up some writers and writers
	for (int i = 0; i < numThreads; ++i) {
		writers[i] =
			std::thread([fs, i] {
			File f = fs->select("TestFile");
			STORAGE::IO::Writer writer = fs->getWriter(f);
			fs->lock(f, STORAGE::IO::EXCLUSIVE);
			{
				writer.write(data[i].c_str(), data[i].size());
			}
			fs->unlock(f, STORAGE::IO::EXCLUSIVE);
		});
		writers[i].detach();
		readers[i] =
			std::thread([&]() {
			File f = fs->select("TestFile");
			STORAGE::IO::Reader reader = fs->getReader(f);
			std::string res;
			bool valid = false;
			fs->lock(f, STORAGE::IO::NONEXCLUSIVE);
			{
				res = reader.readString();
				for (int ind = 0; ind < numThreads; ++ind) {
					if (res.compare(data[ind]) == 0) {
						valid = true;
						break;
					}
				}
			}
			fs->unlock(f, STORAGE::IO::NONEXCLUSIVE);
			failure = !valid;
		});
	}

	for (auto &it : readers) {
		it.join();
	}
	
	fs->toggleMVCC();

	return failure;
}