#include "Testing.h"

int TestUnlink(STORAGE::Filesystem *fs) {
	File &first = fs->select("FirstFile");
	File &second = fs->select("SecondFile");

	std::string f1Data = random_string(128);
	auto f1Writer = fs->getSafeWriter(first);
	f1Writer.write(f1Data.c_str(), f1Data.size());

	std::string f2Data = random_string(32);
	auto f2Writer = fs->getSafeWriter(second);
	f2Writer.write(f2Data.c_str(), f2Data.size());

	STORAGE::FileHeader h1 = fs->getHeader(first);
	STORAGE::FileHeader h2 = fs->getHeader(second);
	FileSize sizeBefore = h2.virtualSize;
	bool merged = fs->unlink(first);
	h2 = fs->getHeader(second);
	if (merged && h2.virtualSize != sizeBefore + STORAGE::FileHeader::SIZE + h1.virtualSize) {
		return 1;
	}
	return 0;
}