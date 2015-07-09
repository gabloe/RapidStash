

#include "Filesystem.h"
#include "Logging.h"

#define VECTOR 0

#if VECTOR
#include <vector>
#else
#include <array>
#endif

const int Max = (2 << 15) - 1;
const int NumThreads = 4;

static std::string data("ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890ABCDEFGHIJKLMNOPQRST");
static std::string filename("MyFile");

void foo(STORAGE::Filesystem *f, int id) {
	File file;
	std::string filename("MyFile");
	std::ostringstream n;

	for (int i = id; i < Max; i += NumThreads) {
		n << i;

		// Create random file
		file = f->select(filename + n.str());
		auto writer = f->getWriter(file);

		// Write lots of data
		f->lock(file, STORAGE::WRITE);
		{
			writer.write(data.c_str(), data.size());
#if EXTRATESTING
			char *buf = reader.read();
			std::string res(buf, f->getSize(file));
			if (data.str().compare(res) == 0) {
				logEvent(EVENT, "Written data verified for file " + filename.str());
			} else {
				logEvent(ERROR, "Written data incorrect for file " + filename.str());
				logEvent(ERROR, "Found '" + res + "', should be '" + data.str() + "'");
			}
#endif
		}
		f->unlock(file, STORAGE::WRITE);
		n.str(std::string());
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
	double throughput = f.count(STORAGE::WRITES) / totalTime;
	double bps = f.count(STORAGE::BYTES) / totalTime;
	std::ostringstream os, os2;
	os << "Turnaround time: " << totalTime << " s";
	if (throughput > 1000) {
		os2 << "Throughput: " << throughput / 1000 << " thousand writes per second. (" << bps / 1024 << " kbytes per second)";
	} else {
		os2 << "Throughput: " << throughput << " writes per second. (" << bps << " bytes per second)";
	}

#if VECTOR
	threads.clear();
#endif

	f.shutdown();

#if LOGGING
	logEvent(EVENT, os.str());
	logEvent(EVENT, os2.str());
#else
	std::cout << os.str() << std::endl;
	std::cout << os2.str() << std::endl;
	std::cout << "Press enter to continue..." << std::endl;
	std::getchar();
#endif

	return 0;
}
