#include "Filesystem.h"
#include "Logging.h"

int main() {
	STORAGE::Filesystem f("test.txt");
	STORAGE::File *file = f.createNewFile("MyFile1");
	delete file;
	f.shutdown();
	return 0;
}