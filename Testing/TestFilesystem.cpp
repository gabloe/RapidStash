#include "Filesystem.h"
#include "Logging.h"

void foo(STORAGE::Filesystem *f, std::string name) {
	STORAGE::File file = f->select(name);
	f->lock(file);
	f->unlock(file);
}

int main() {
	STORAGE::Filesystem f("test.txt");
	std::thread t1(foo, &f, "MyFile1");
	std::thread t2(foo, &f, "MyFile1");
	t1.join();
	t2.join();
	f.shutdown();
	return 0;
}