#include "Testing.h"
#include <vector>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>

const char DIR_SEPARATOR = '\\';
#else
const char DIR_SEPARATOR = '/';
#endif

std::string ConvertLastErrorToString(void) {
	auto errorMessageID = GetLastError();
	if (errorMessageID == 0) return std::string();

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string result(messageBuffer, size);
	
	LocalFree(messageBuffer);

	return result;
}

std::string getCurrentDirectory() {
	char buffer[_MAX_DIR];
	size_t len = GetCurrentDirectory(512, buffer);
	return std::string(buffer, len);
}

void testDelete(std::string directory) {
	directory = getCurrentDirectory() + DIR_SEPARATOR + directory + DIR_SEPARATOR + "*";
	SHFILEOPSTRUCT opt;
	memset(&opt, 0, sizeof(SHFILEOPSTRUCT));
	opt.wFunc = FO_DELETE;
	opt.pFrom = directory.c_str();

	std::cout << "Removing " << directory << std::endl;

	if (!SHFileOperation(&opt)) {
		std::cout << "ERROR: " << ConvertLastErrorToString() << std::endl;
	}
}

// Given a directory remove the files located within it and then the directory itself
void removeDirectory(std::string directory) {
	std::string CWD = getCurrentDirectory();
	directory = CWD + DIR_SEPARATOR + directory;

	WIN32_FIND_DATA data;
	HANDLE h = FindFirstFile((directory + DIR_SEPARATOR + "*").c_str(), &data);
	if (h != NULL) {
		SetFileAttributes(directory.c_str(), FILE_ATTRIBUTE_NORMAL);
		FindNextFile(h, &data); // ..
		FindNextFile(h, &data); // .
		do {
			std::string filename = directory + DIR_SEPARATOR + data.cFileName;
			if (!SetFileAttributes(filename.c_str(), FILE_ATTRIBUTE_NORMAL)) {
				std::cout << "ERROR: " << ConvertLastErrorToString() << std::endl;
				std::cout << "File: " << filename << std::endl;
			}
			BOOL result = DeleteFileA(filename.c_str());
			if (result == 0) {
				std::cout << "ERROR: " << ConvertLastErrorToString() << std::endl;
				std::cout << "File: " << filename << std::endl;
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

	SECURITY_ATTRIBUTES attr;
	attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	attr.bInheritHandle = true;
	attr.lpSecurityDescriptor = NULL;

	if (!CreateDirectory("data", &attr)) {
		std::cout << "ERROR: " << ConvertLastErrorToString() << std::endl;
		std::cout << "Could not create data directory" << std::endl;
		_mkdir("data");
	}

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

	//removeDirectory("data");
	testDelete("data");
}

const size_t NumTests = 1;

int main() {
	for (size_t i = 0; i < NumTests; ++i) test();
	return 0;
}