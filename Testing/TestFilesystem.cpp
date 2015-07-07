#include "Filesystem.h"
#include "Logging.h"

void foo(STORAGE::Filesystem *f, std::string name) {
	STORAGE::File file = f->select(name);
	auto writer = f->getWriter(file);
	auto reader = f->getReader(file);

	f->lock(file, STORAGE::WRITE);

	std::thread::id this_id = std::this_thread::get_id();
	std::ostringstream os;
	os << "Thread # " << this_id;
	std::string data = os.str();
	writer.write(data.c_str(), data.size());

	f->unlock(file, STORAGE::WRITE);

	return;
}

int main() {
	STORAGE::Filesystem f("test.stash");

	// Create 50 threads, concurrently write to 5 files.
	std::thread threads[50];
	srand((unsigned int)time(NULL));
	for (int i = 0; i < 50; ++i) {
		std::ostringstream os;
		os << "MyFile" << rand() % 5;
		threads[i] = std::thread(foo, &f, os.str());
	}
	
	for (auto& th : threads) {
		th.join();
	}

	f.shutdown();
	return 0;
}