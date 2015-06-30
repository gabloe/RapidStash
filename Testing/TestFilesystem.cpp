#include "Filesystem.h"
#include "Logging.h"

int main() {
	STORAGE::Filesystem f("test.txt");
	for (int i = 0; i < 10; ++i)
	{
		STORAGE::File *file = f.createNewFile("MyFile1");
		delete file;
	}
	f.shutdown();
	return 0;
}