#include "Testing.h"
#include <vector>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#endif

int main() {
	std::vector<TestWrapper_t> fn;
	fn.push_back([] {return TestWrapper("TestReadWrite", TestReadWrite); });
	fn.push_back([] {return TestWrapper("TestHeader", TestHeader); });
	fn.push_back([] {return TestWrapper("TestConcurrentWrite", TestConcurrentWrite); });
	fn.push_back([] {return TestWrapper("TestConcurrentReadWrite", TestConcurrentReadWrite); });
	fn.push_back([] {return TestWrapper("TestConcurrentMultiFile", TestConcurrentMultiFile); });
	fn.push_back([] {return TestWrapper("TestMVCC", TestMVCC); });
	fn.push_back([] {return TestWrapper("TestConcurrentMultiFileMVCC", TestConcurrentMultiFileMVCC); });
	fn.push_back([] {return TestWrapper("TestUnlink", TestUnlink); });

	_mkdir("data");

	std::cout << "Test Results:" << std::endl;
	std::cout << std::setfill('-');
	for (auto &func : fn) {
		func();
	}

#if defined(_WIN32) || defined(_WIN64)
	std::cout << "\nPress enter to continue..." << std::endl;
	std::getchar();
#endif

	_rmdir("data");

	return 0;
}