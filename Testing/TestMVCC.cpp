#include "Filewriter.h"
#include "Filesystem.h"
#include "Testing.h"

#include <string>
#include <iostream>
#include <array>
#include <thread>

const int numThreads = 16;
static bool failure = false;

static std::string data[numThreads];

int TestMVCC(STORAGE::Filesystem *fs) {

	for (int i = 0; i < numThreads; ++i) {
		std::srand((unsigned int)std::time(NULL) + i);
		data[i] = random_string(64);
	}

	fs->toggleMVCC();
	std::array<std::thread, numThreads> writers;
	std::array<std::thread, numThreads> readers;

	// Start up some writers
	for (int i = 0; i < numThreads; ++i) {
		writers[i] =
			std::thread([fs, &i]() {
			if (data[i].size() == 0) {
				std::cout << "OH NOES!!!" << std::endl;
				return;
			}
			File f = fs->select("TestFile");
			STORAGE::IO::Writer writer = fs->getWriter(f);
			fs->lock(f, STORAGE::IO::EXCLUSIVE);
			{
				writer.write(data[i].c_str(), data[i].size());
			}
			fs->unlock(f, STORAGE::IO::EXCLUSIVE);
		});
	}

	for (auto &it : writers) {
		it.join();
	}

	// Start up some readers
	for (int i = 0; i < numThreads; ++i) {
		readers[i] =
			std::thread([fs]() {
			File f = fs->select("TestFile");
			STORAGE::IO::Reader reader = fs->getReader(f);
			fs->lock(f, STORAGE::IO::NONEXCLUSIVE);
			{
				std::string res = reader.readString();
				bool valid = false;
				for (size_t ind = 0; ind < numThreads; ++ind) {
					if (res.compare(data[ind]) == 0) {
						valid = true;
						break;
					}
				}
				failure = !valid;
				if (failure) {
					std::cout << "Read : " << res << std::endl;
				}
			}
			fs->unlock(f, STORAGE::IO::NONEXCLUSIVE);
		});
	}

	for (auto &it : readers) {
		it.join();
	}
	fs->toggleMVCC();

	return failure;
}