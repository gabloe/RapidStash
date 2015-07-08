
#include "Filesystem.h"
#include "Logging.h"

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
	static int c;
	using namespace std::literals;
	STORAGE::Filesystem f("test.stash");

	// Create 50 threads, concurrently write to 5 files.
	std::thread threads[50];
	for (int j = 0; j < 50000; ++j) {
		srand((unsigned int)time(NULL));
		for (int i = 0; i < 50; ++i) {
			std::ostringstream os;
			os << "MyFile" << rand();
			threads[i] = std::thread(foo, &f, os.str());
		}
		
		for (auto& th : threads) {
			th.join();
		}

		std::cout << j << std::endl;
	}
	f.shutdown();
	return 0;
}
