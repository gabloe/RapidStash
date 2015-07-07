#include "Filesystem.h"
#include "Logging.h"

void foo(STORAGE::Filesystem *f, std::string name) {
	STORAGE::File file = f->select(name);
	auto writer = f->getWriter(file);
	auto reader = f->getReader(file);

	f->lock(file);

	std::thread::id this_id = std::this_thread::get_id();
	std::ostringstream os;
	os << this_id;
	std::string data("Thread #" + os.str());
	writer.write(data.c_str(), data.size());
	try {
		char *res;
		res = reader.read();
		std::string str(res, f->getSize(file));
		std::ostringstream os2;
		os2 << data.compare(str);
		logEvent(EVENT, "Wrote " + data + ", read " + str + " same? " + os2.str());
		free(res);
	} catch (STORAGE::SeekOutOfBoundsException &e) {
		logEvent(ERROR, "Error reading file");
	}

	f->unlock(file);
}

int main() {
	STORAGE::Filesystem f("test.txt");
	std::thread threads[100];
	for (int i = 0; i < 100; ++i) {
		std::ostringstream os;
		os << i;
		threads[i] = std::thread(foo, &f, "MyFile1");
	}
	
	for (auto& th : threads) {
		th.join();
	}

	f.shutdown();
	return 0;
}