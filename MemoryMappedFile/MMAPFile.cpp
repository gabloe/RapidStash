/*
*  DynamicMemoryMappedFile.cpp
*  Wraps memory mapped file creation with dynamic growth.  Manages when to grow the underlying file and provides
*  raw unmanaged read and write access.
*
*  Written by: Gabriel J. Loewen
*/


#include "MMAPFile.h"
#include "Logging.h"


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

// Trying to support cross compatibility
#if defined(_WIN32) || defined(_WIN64)
int ftruncate(int fd, size_t len) {
	return _chsize_s(fd, len);
}
#endif

// Test for file existence
bool fileExists(const char* fname) {
	struct stat buffer;
	return (stat(fname, &buffer) == 0);
}

/*
 * Constructor!
 */
STORAGE::DynamicMemoryMappedFile::DynamicMemoryMappedFile(const char* fname) : backingFilename(fname) {
	// If the backing file does not exist, we need to create it
	bool createInitial;

	bool exists = fileExists(backingFilename);

	fHandle = getFileDescriptor(backingFilename, !exists);
	fd = _open_osfhandle((intptr_t)fHandle, _O_WRONLY);

	isNewFile = !exists;
	createInitial = !exists;

	if (!exists) {
		createInitial = true;
		mapSize = INITIAL_SIZE;
	} else {
		// Read file size from the host filesystem
		mapSize = GetFileSize(fHandle, NULL);
		logEvent(EVENT, "Detected map size of " + toString(mapSize));
	}

	fs = (char*)mmap((void*)NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
	if (fs == MAP_FAILED) {
		logEvent(ERROR, "Could not map backing file");
		shutdown(FAILURE);
	}

	if (createInitial) {
		logEvent(EVENT, "Creating initial file structure");
		SetFilePointer(fHandle, mapSize, NULL, FILE_BEGIN);
		SetEndOfFile(fHandle);
		SetFilePointer(fHandle, 0, NULL, FILE_BEGIN);
		writeHeader();
	} else {
		logEvent(EVENT, "Reading file structure");
		// Read header, perform sanity check, remap.
		const char *header = readHeader();
		if (!sanityCheck(header)) {
			// Uhoh...
			logEvent(ERROR, "Sanity check failed");
			shutdown(FAILURE);
		}
		// Extract version and size.
		short recordedVersion;
		memcpy(&recordedVersion, header + sizeof(SANITY), sizeof(VERSION));
		// If the version differs than there could be some compatibility issues...
		if (recordedVersion != VERSION) {
			// What do?
			logEvent(ERROR, "Version mismatch");
			shutdown(FAILURE);
		}

#if EXTRATESTING
		// Verify size matches recorded size from header.  If mismatched then
		// potentially we lost data on the last write.
		size_t msize;
		memcpy(&msize, header + sizeof(SANITY) + sizeof(VERSION), sizeof(msize));

		// mmap over the previous region
		if (msize != mapSize) {
			logEvent(EVENT, "File size mismatch, read " + toString(msize) + ", should be " + toString(mapSize));
		}
#endif
		// Cleanup
		free((void*)header);
	}
}

/*
 * Public Methods
 */

int STORAGE::DynamicMemoryMappedFile::shutdown(const int code) {
	logEvent(EVENT, "Shutting down with code " + toString(code));
	if (code != SUCCESS) {
		logEvent(ERROR, "Shutdown failure");
		exit(code);
	}
	
	writeHeader();

	if (munmap(fs, mapSize)) {
		logEvent(ERROR,"ERROR (CloseHandle): " + ConvertLastErrorToString());
	}
	
	if (!CloseHandle(fHandle)) {
		logEvent(ERROR,"ERROR (CloseHandle): " + ConvertLastErrorToString());
	}

	if (logOut.is_open()) {
		logOut.close();
	}

	return code;
}

const size_t MaxFileSize = 1 << 30; // No file can be more than a GB

#include <cassert>

int STORAGE::DynamicMemoryMappedFile::raw_write(const char *data, size_t len, size_t pos) {

	assert(len <= MaxFileSize);

	// If we are trying to write beyond the end of the file, we must grow.
	size_t start = pos + HEADER_SIZE;
	size_t end = start + len;

	{
		std::unique_lock<std::mutex> lk(growthLock);
		if (end > mapSize) {
			grow(end);
		}
		memcpy(fs + start, data, len);
	}
	
	return 0;
}

char *STORAGE::DynamicMemoryMappedFile::raw_read(size_t pos, size_t len, size_t off) {
	size_t start = pos + off;
	size_t end = start + len;

	if (end > mapSize) {
		// Crash gently...
		logEvent(ERROR, "Attempted to read beyond the end of the filesystem!");
		shutdown(FAILURE);
	}

	char *data = NULL;

	if (len > 1000000000) {
		logEvent(ERROR, "Attempting to allocate to much space");
		shutdown(FAILURE);
	}
	
	data = (char *)malloc(len);
	if (data != NULL) {
		{
			std::unique_lock<std::mutex> lk(growthLock);
			if (end > mapSize) {
				grow(end);
			}
			memcpy(data, fs + start, len);
		}
	}

	return data;
}

/*
 * Private Methods
 */

HANDLE STORAGE::DynamicMemoryMappedFile::getFileDescriptor(const char *fname, bool create) {
	if (create) {
		fHandle = CreateFile(fname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	} else {
		fHandle = CreateFile(fname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING , FILE_ATTRIBUTE_NORMAL, NULL);
	}

	if (fHandle == INVALID_HANDLE_VALUE) {
		logEvent(ERROR, "Unable to open backing file, aborted with error " + toString(fHandle));
		shutdown(FAILURE);
	}
	else {
		logEvent(EVENT, "Create the file " + std::string(fname));
	}
	return fHandle;
}

void STORAGE::DynamicMemoryMappedFile::writeHeader() {
	logEvent(EVENT, "Updating file header");
	size_t msize = mapSize;
	memcpy(fs, SANITY, sizeof(SANITY));
	memcpy(fs + sizeof(SANITY), reinterpret_cast<char*>(&VERSION), sizeof(VERSION));
	memcpy(fs + sizeof(SANITY) + sizeof(VERSION), reinterpret_cast<char*>(&msize), sizeof(msize));
	msync(fs, HEADER_SIZE, MS_SYNC);
}

char *STORAGE::DynamicMemoryMappedFile::readHeader() {
	char *header = raw_read(0, HEADER_SIZE, 0);
	return header;
}

bool STORAGE::DynamicMemoryMappedFile::sanityCheck(const char * header) {
	for (int i = 0; i < sizeof(SANITY); ++i) {
		if (header[i] != SANITY[i]) {
			return false;
		}
	}
	return true;
}

size_t STORAGE::DynamicMemoryMappedFile::align(size_t amt) {
	const short alignment = 16;
	size_t rem = amt % alignment;
	return amt + alignment - rem;
}

void STORAGE::DynamicMemoryMappedFile::grow(size_t newSize) {	// Increase the size by some amount
	size_t oldMapSize = mapSize;
	size_t amt = newSize - oldMapSize;
	if (amt <= 0) {
		return;
	}
	size_t test = (size_t)std::ceil(newSize * GROWTH_FACTOR);
	mapSize = align(test > maxSize ? maxSize : test);

#if EXTRATESTING
	logEvent(EVENT, "Growing filesystem to " + toString(mapSize));
#endif

	SetFilePointer(fHandle, mapSize, NULL, FILE_BEGIN);
	SetEndOfFile(fHandle);
	SetFilePointer(fHandle, 0, NULL, FILE_BEGIN);
	munmap(fs, mapSize);
	fs = (char*)mmap((void*)NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED,fd, 0);
	
	if (fs == MAP_FAILED) {
		// Uhoh...
		logEvent(ERROR, "Could not remap backing file after growing");
		shutdown(FAILURE);
	}
}
