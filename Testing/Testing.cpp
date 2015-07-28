#include "Testing.h"
#include <vector>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#endif

int main() {
	std::vector<TestWrapper_t> fn;
	fn.push_back([] {return TestWrapper("Read Write", TestReadWrite); });
	fn.push_back([] {return TestWrapper("File Header", TestHeader); });
	fn.push_back([] {return TestWrapper("Concurrent Write", TestConcurrentWrite); });
	fn.push_back([] {return TestWrapper("Concurrent Read Write", TestConcurrentReadWrite); });
	fn.push_back([] {return TestWrapper("Concurrent Multi-File", TestConcurrentMultiFile); });
	fn.push_back([] {return TestWrapper("MVCC", TestMVCC); });
	fn.push_back([] {return TestWrapper("Concurrent Multi-File MVCC", TestConcurrentMultiFileMVCC); });
	fn.push_back([] {return TestWrapper("Unlink", TestUnlink); });

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