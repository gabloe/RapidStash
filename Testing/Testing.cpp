#include "Testing.h"
#include <vector>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#endif

std::string ConvertErrorToString(BOOL errorMessageID) {
	if (errorMessageID == 0) return std::string();

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string result(messageBuffer, size);

	LocalFree(messageBuffer);

	return result;
}

void removeDirectory(std::string directory) {

	char buffer[512];

	size_t len = GetCurrentDirectory(512, buffer);

	std::string CWD(buffer, len);

	directory = CWD + "\\" + directory;

	WIN32_FIND_DATA data;
	HANDLE h = FindFirstFile((directory + "\\*").c_str(), &data);
	if (h) {
		do {
			std::string filename = CWD + "\\" + data.cFileName;
			BOOL result = DeleteFileA(filename.c_str());
			switch (result) {
				case ERROR_FILE_NOT_FOUND:
					std::cout << "File not found: " << filename << std::endl;
					break;
				case ERROR_ACCESS_DENIED:
					std::cout << "Access denied for file: " << filename << std::endl;
					break;
				default:
					if(result != 0) {
						std::cout << "Error: " << ConvertErrorToString(result) << std::endl;
					}
			}
		} while (FindNextFile(h,&data));
		FindClose(h);
	}
	_rmdir(directory.c_str());

}

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

	removeDirectory("data");

}

const size_t NumTests = 16;

int main() {
	for (size_t i = 0; i < NumTests; ++i) test();
	return 0;
}