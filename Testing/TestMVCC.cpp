#include "Testing.h"

int TestMVCC(STORAGE::Filesystem *fs) {
	fs->toggleMVCC();
	int res = TestConcurrentReadWrite(fs);
	fs->toggleMVCC();
	return res;
}