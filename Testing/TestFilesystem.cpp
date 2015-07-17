#include "Filesystem.h"
#include "Logging.h"
#include "Filereader.h"
#include "Filewriter.h"

#define VECTOR 0

#if VECTOR
#include <vector>
#else
#include <array>
#endif

const int Max = 2 << 15;
const int NumThreads = 12;

std::string prefix("ABCDEFGHIJKLMNOPQRTUVWXYZ");

std::string random_string(size_t length)
{
	auto randchar = []() -> char
	{
		const char charset[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[rand() % max_index];
	};
	std::string str(length, 0);
	std::generate_n(str.begin(), length, randchar);
	return str;
}

void bar(STORAGE::Filesystem *f, int id) {
	using namespace std::literals;

	File file;
	std::ostringstream n;

	for (int i = id; i < Max; i += NumThreads) {
		n << i;
		std::string filename("MyFile" + n.str());

		// Create file
		file = f->select(filename);
		auto reader = f->getReader(file);

		// Read lots of data
		f->lock(file, STORAGE::WRITELOCK);
		{
			char *d = reader.read();
#if EXTRATESTING
			std::string data(prefix + filename);
			std::string res(d, f->getHeader(file).size);
			if (data.compare(res) == 0) {
				logEvent(EVENT, "Read data verified for file " + filename);
			} else {
				logEvent(ERROR, "Read data incorrect for file " + filename);
				logEvent(ERROR, "Found '" + res + "', should be '" + data + "'");
				std::getchar();
				f->shutdown(FAILURE);
			}
#endif
			free(d);
		}
		f->unlock(file, STORAGE::WRITELOCK);
		n.str(std::string());
	}
}

void foo(STORAGE::Filesystem *f, int id) {
	File file;
	std::ostringstream n;

	for (int i = id; i < Max; i += NumThreads) {
		std::string tmp = random_string(std::rand() % 512 + 32);
		n << i;
		std::string filename("MyFile" + n.str());
		std::string data(prefix + filename);

		// Create random file
		file = f->select(filename);
		auto writer = f->getWriter(file);

		// Write lots of data
		f->lock(file, STORAGE::WRITELOCK);
		{
			writer.write(data.c_str(), data.size());
#if EXTRATESTING
			auto reader = f->getReader(file);
			char *buf = reader.read();
			std::string res(buf, f->getHeader(file).size);
			if (data.compare(res) == 0) {
				logEvent(EVENT, "Written data verified for file " + filename);
			} else {
				logEvent(ERROR, "Written data incorrect for file " + filename);
				logEvent(ERROR, "Found '" + res + "', should be '" + data + "'");
				std::getchar();
				f->shutdown(FAILURE);
			}
			free(buf);
#endif
		}
		f->unlock(file, STORAGE::WRITELOCK);
		n.str(std::string());
	}
	return;
}


int main() {
	std::srand((unsigned int)std::time(NULL));
#if !LOGGING
	std::cout << "Working..." << std::endl;
#endif
	STORAGE::Filesystem f("test.stash");
#if VECTOR
	std::vector<std::thread> writers;
	std::vector<std::thread> readers;
#else
	std::array<std::thread, NumThreads> writers;
	std::array<std::thread, NumThreads> readers;
#endif

	for (int i = 0; i < NumThreads; ++i) {
#if VECTOR
		writers.push_back(std::thread(foo, &f, i));
#else
		writers[i] = std::thread(foo, &f, i);
#endif
	}

	for (int i = 0; i < NumThreads; ++i) {
#if VECTOR
		readers.push_back(std::thread(bar, &f, i));
#else
		readers[i] = std::thread(foo, &f, i);
#endif
	}

	for (auto& th : writers) {
		th.join();
	}

	for (auto& th : readers) {
		th.join();
	}

	size_t numWrites = f.count(STORAGE::NUMWRITES);
	size_t numBytesWrote = f.count(STORAGE::BYTESWRITTEN);
	size_t numReads = f.count(STORAGE::NUMREADS);
	size_t numBytesRead = f.count(STORAGE::BYTESREAD);

	double writeThroughput = f.getThroughput(STORAGE::NUMWRITES);
	double bpsWrote = f.getThroughput(STORAGE::BYTESWRITTEN);

	double readThroughput = f.getThroughput(STORAGE::NUMREADS);
	double bpsRead = f.getThroughput(STORAGE::BYTESREAD);

	std::ostringstream os, os2, os3, os4, os5;
	os << "Total write time: " << f.getThroughput(STORAGE::WRITETIME) << " seconds." << std::endl;
	os << "Total read time: " << f.getThroughput(STORAGE::READTIME) << " seconds." << std::endl;

	if (writeThroughput > 1000) {
		os2 << "Throughput: " << writeThroughput / 1000 << " thousand writes per second. (" << bpsWrote / 1024 << " kbytes per second)";
	}
	else {
		os2 << "Throughput: " << writeThroughput << " writes per second. (" << bpsWrote << " bytes per second)";
	}
	os3 << "Wrote " << numBytesWrote << " bytes in " << numWrites << " write operations";

	if (readThroughput > 1000) {
		os4 << "Throughput: " << readThroughput / 1000 << " thousand reads per second. (" << bpsRead / 1024 << " kbytes per second)";
	}
	else {
		os4 << "Throughput: " << readThroughput << " reads per second. (" << bpsRead << " bytes per second)";
	}
	os5 << "Read " << numBytesRead << " bytes in " << numReads << " read operations";

#if VECTOR
	writers.clear();
	readers.clear();
#endif

#if EXTRATESTING
	f.checkFreeList();
#endif

	f.shutdown();

#if LOGGING
	logEvent(EVENT, os.str());
	logEvent(EVENT, os2.str());
#else
	std::cout << "\nStatistics:\n" << std::endl;
	std::cout << os.str() << std::endl;

	std::cout << "\n--- Writes ---\n";
	std::cout << os2.str() << std::endl;
	std::cout << os3.str() << std::endl;

	std::cout << "\n--- Reads ---\n";
	std::cout << os4.str() << std::endl;
	std::cout << os5.str() << std::endl;
	std::cout << "\nPress enter to continue..." << std::endl;
	(void)std::getchar();
#endif

	return 0;
}