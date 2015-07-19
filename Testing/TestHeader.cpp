#include "Filewriter.h"
#include "Filesystem.h"
#include "Testing.h"

std::string data("Hello, World!");

// Write a file and verify the file header

int TestHeader(STORAGE::Filesystem *fs) {
	File f = fs->select("TestFile");
	STORAGE::IO::Writer writer = fs->getWriter(f);
	fs->lock(f, STORAGE::IO::EXCLUSIVE);
	{
		writer.write(data.c_str(), data.size());
	}
	fs->unlock(f, STORAGE::IO::EXCLUSIVE);

	STORAGE::FileHeader header = fs->getHeader(f);

	if (header.size != data.size() ||
		strcmp("TestFile", header.name) != 0 ||
		header.version != 0) {
		return -1;
	}

	return 0;
}