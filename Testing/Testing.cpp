#include "Testing.h"
#include <vector>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#endif

void test() {
	std::vector<TestWrapper_t> fn;
	//fn.push_back([] { TestWrapper("Read Write", TestReadWrite); });
	//fn.push_back([] { TestWrapper("File Header", TestHeader); });
	//fn.push_back([] { TestWrapper("Concurrent Write", TestConcurrentWrite); });
	//fn.push_back([] { TestWrapper("Concurrent Read Write", TestConcurrentReadWrite); });
	//fn.push_back([] { TestWrapper("Concurrent Multi-File", TestConcurrentMultiFile); });
	//fn.push_back([] { TestWrapper("MVCC", TestMVCC); });
	fn.push_back([] { TestWrapper("Concurrent Multi-File MVCC", TestConcurrentMultiFileMVCC); });
	fn.push_back([] { TestWrapper("Unlink", TestUnlink); });

	_mkdir("data");

	std::cout << "Test Results:" << std::endl;
	std::cout << std::setfill('-');
	for (auto &func : fn) {
		func();
	}
	std::cout << std::setfill(' ');

#if defined(_WIN32) || defined(_WIN64)
	//std::cout << "\nPress enter to continue..." << std::endl;
	//std::getchar();
#endif

	_rmdir("data");

}

int main() {
	for (int i = 0; i < 1024; ++i) test();
	return 0;
}