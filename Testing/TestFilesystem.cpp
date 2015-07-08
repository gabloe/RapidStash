

#include "Filesystem.h"
#include "Logging.h"

#include <chrono>

#define VECTOR 0

#if VECTOR
#include <vector>
#else
#include <array>
#endif

const int Max = 50000;
const int NumThreads = 50;


void foo(STORAGE::Filesystem *f, int id) {
	for (int i = id; i < Max; i += NumThreads) {

		// Create random file
		std::ostringstream filename;
		filename << "MyFile" << i;
		STORAGE::File file = f->select(filename.str());
		auto writer = f->getWriter(file);

		// Create random data
		std::ostringstream data;
		data << "Thread # " << i;

		f->lock(file, STORAGE::WRITE);
		{
			writer.write(data.str().c_str(), data.str().size());
		}
		f->unlock(file, STORAGE::WRITE);

		if (i % 100 == 0) {
			std::cout << "Just finished " << i << std::endl;
		}
	}
	return;
}

int main() {
	using namespace std::literals;
	STORAGE::Filesystem f("test.stash");
#if VECTOR
  std::vector<std::thread> threads;
#else
	std::array<std::thread,NumThreads> threads;
#endif
	
	srand((unsigned int)time(NULL));

	std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
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

	std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double> turnaround = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
	const double totalTime = turnaround.count();
	double throughput = (Max * NumThreads) / totalTime;
	std::ostringstream os, os2;
	os << "Turnaround time: " << totalTime << " seconds" << std::endl;
	os2 << "Throughput: " << throughput << " writes per second" << std::endl;
	logEvent(EVENT, os.str());
	logEvent(EVENT, os2.str());

#if VECTOR
	threads.clear();
#endif
	
	

	f.shutdown();
	return 0;
}
