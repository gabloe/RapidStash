

#include "Filesystem.h"
#include "Logging.h"

#define VECTOR 0

#if VECTOR
#include <vector>
#else
#include <array>
#endif

const int Max = (2 << 18) - 1;
const int NumThreads = 4;

static std::string data("ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQRST");
static std::string filename("MyFile");

void foo(STORAGE::Filesystem *f, int id) {
	File file;
	std::ostringstream filename;
	filename << "MyFile";

	for (int i = id; i < Max; i += NumThreads) {
		filename << i;
		// Create random file
		file = f->select(filename.str());
		auto writer = f->getWriter(file);

		// Write lots of data
		f->lock(file, STORAGE::WRITE);
		{
			writer.write(data.c_str(), data.size());
#if EXTRATESTING
			auto reader = f->getReader(file);
			char *buf = reader.read();
			std::string res(buf, f->getHeader(file).size);
			if (data.compare(res) == 0) {
				logEvent(EVENT, "Written data verified for file " + filename.str());
			} else {
				logEvent(ERROR, "Written data incorrect for file " + filename.str());
				logEvent(ERROR, "Found '" + res + "', should be '" + data + "'");
			}
#endif
		}
		f->unlock(file, STORAGE::WRITE);
		filename.str(std::string("MyFile"));
	}
	return;
}

int main() {
#if !LOGGING
	std::cout << "Working..." << std::endl;
#endif
	STORAGE::Filesystem f("test.stash");
#if VECTOR
  std::vector<std::thread> threads;
#else
	std::array<std::thread,NumThreads> threads;
#endif

	for (int i = 0; i < NumThreads; ++i) {
#if VECTOR
		threads.push_back(std::thread(foo, &f, i));
#else
		threads[i] = std::thread(foo, &f, i);
#endif
	}

	for (auto& th : threads) {
		th.join();
	}

	const double totalTime = f.getTurnaround();
	size_t numWrites = f.count(STORAGE::WRITES);
	size_t numBytes = f.count(STORAGE::BYTES);

	double throughput =  numWrites / totalTime;
	double bps = numBytes / totalTime;
	std::ostringstream os, os2, os3;
	os << "Turnaround time: " << totalTime << " s";
	if (throughput > 1000) {
		os2 << "Throughput: " << throughput / 1000 << " thousand writes per second. (" << bps / 1024 << " kbytes per second)";
	} else {
		os2 << "Throughput: " << throughput << " writes per second. (" << bps << " bytes per second)";
	}
	os3 << "Wrote " << numBytes << " bytes in " << numWrites << " write operations";

#if VECTOR
	threads.clear();
#endif

	f.shutdown();

#if LOGGING
	logEvent(EVENT, os.str());
	logEvent(EVENT, os2.str());
#else
	std::cout << "\nStatistics:" << std::endl;
	std::cout << os.str() << std::endl;
	std::cout << os2.str() << std::endl;
	std::cout << os3.str() << std::endl;
	std::cout << "\nPress enter to continue..." << std::endl;
	(void)std::getchar();
#endif

	return 0;
}
