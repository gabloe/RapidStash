#include "Testing.h"

int TestConcurrentMultiFileMVCC(STORAGE::Filesystem *fs) {
	fs->toggleMVCC();
	int res = TestConcurrentMultiFile(fs);
	fs->toggleMVCC();
	return res;
}