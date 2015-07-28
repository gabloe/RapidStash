#include "Filewriter.h"
#include "Filesystem.h"
#include "Testing.h"

#include <string>
#include <iostream>

int TestReadWrite(STORAGE::Filesystem *fs) {
	static unsigned int test;

	File &file = fs->select("TestFile");
	STORAGE::IO::SafeWriter writer = fs->getSafeWriter(file);
	STORAGE::IO::SafeReader reader = fs->getSafeReader(file);

	std::srand(test++);
	std::string data = random_string(dataSize);

	writer.write(data.c_str(), data.size());
	std::string res = reader.readString();
	if (res.compare(data) != 0) {
		return -1;
	}

	return 0;
}