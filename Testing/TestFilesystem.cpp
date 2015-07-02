#include "Filesystem.h"
#include "Logging.h"

int main() {
	STORAGE::Filesystem f("test.txt");
	STORAGE::File file = f.select("MyFile1");
	f.shutdown();
	return 0;
}