

#include "Filesystem.h"
#include "Logging.h"


#define VECTOR 0

#if VECTOR
#include <vector>
#else
#include <array>
#endif


void foo(STORAGE::Filesystem *f, std::string name) {
	STORAGE::File file = f->select(name);
	auto writer = f->getWriter(file);
	std::thread::id this_id = std::this_thread::get_id();
	std::ostringstream os;
	os << "Thread # " << this_id;
	std::string data = os.str();

	f->lock(file, STORAGE::WRITE);
  {
	  writer.write(data.c_str(), data.size());
  }
	f->unlock(file, STORAGE::WRITE);

	return;
}

int main() {
	const int NumThreads = 50;
	static int c;
	using namespace std::literals;
	STORAGE::Filesystem f("test.stash");
#if VECTOR
  std::vector<std::thread> threads;
#else
	std::array<std::thread,NumThreads> threads;
#endif
	for (int j = 0; j < 50000; ++j) {

		srand((unsigned int)time(NULL));
		for (int i = 0; i < NumThreads; ++i) {
			std::ostringstream os;
			os << "MyFile" << rand();
#if VECTOR
			threads.push_back(std::thread(foo, &f, os.str()));
#else
			threads[i] = std::thread(foo, &f, os.str());
#endif
		}

		for (auto& th : threads) {
			th.join();
		}
#if VECTOR
		threads.clear();
#endif
		if (j % 100 == 0) {
			std::cout << j << std::endl;
		}
	}
	f.shutdown();
	return 0;
}
