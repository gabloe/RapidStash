/*
*  DynamicMemoryMappedFile.cpp
*  Wraps memory mapped file creation with dynamic growth.  Manages when to grow the underlying file and provides
*  raw unmanaged read and write access.
*
*  Written by: Gabriel J. Loewen
*/

#include "MMAPFile.h"
#include "Logging.h"

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
	growing = false;

	int fd;
	bool exists = fileExists(backingFilename);
	fd = getFileDescriptor(backingFilename, !exists);
	isNewFile = !exists;
	createInitial = !exists;

	if (!exists) {
		createInitial = true;
		mapSize = INITIAL_SIZE;
	} else {
		// Read file size from the host filesystem
		_lseek(fd, 0L, SEEK_END);
		mapSize = _tell(fd);
		_lseek(fd, 0L, SEEK_SET);
		std::ostringstream os;
		os << mapSize;
		logEvent(EVENT, "Detected map size of " + os.str());
	}

	fs = (char*)mmap((void*)NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fs == MAP_FAILED) {
		logEvent(ERROR, "Could not map backing file");
		_close(fd);
		shutdown(FAILURE);
	}

	if (createInitial) {
		logEvent(EVENT, "Creating initial file structure");
		ftruncate(fd, mapSize);
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
			std::ostringstream os, os2;
			os << msize;
			os2 << mapSize;
			logEvent(EVENT, "File size mismatch, read " + os.str() + ", should be " + os2.str());
		}
#endif
		// Cleanup
		free((void*)header);
	}

	// We don't actually need the file descriptor any longer
	_close(fd);
}

/*
 * Public Methods
 */

int STORAGE::DynamicMemoryMappedFile::shutdown(const int code) {
	std::stringstream codeStr;
	codeStr << code;
	logEvent(EVENT, "Shutting down with code " + codeStr.str());
	if (code != SUCCESS) {
		logEvent(ERROR, "Shutdown failure");
		exit(code);
	}
	writeHeader();

	munmap(fs, mapSize);

	if (logOut.is_open()) {
		logOut.close();
	}

	return code;
}

int STORAGE::DynamicMemoryMappedFile::raw_write(const char *data, size_t len, size_t pos) {
	// If we are trying to write beyond the end of the file, we must grow.
	size_t start = pos + HEADER_SIZE;
	size_t end = start + len;

checkspace:
	std::unique_lock<std::mutex> lk(growthLock);
	if (end > mapSize) {
		cvWrite.wait(lk, [] {return !growing; });
		grow(end);
		lk.unlock();
		cvWrite.notify_one();
		goto checkspace;
	} else {
		lk.unlock();
		// If the space is available we can write immediately.
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
	// We cannot read when we are growing.  This prevents out of bound reads.
	std::unique_lock<std::mutex> lk(growthLock);
	cvRead.wait(lk, [] {return !growing; });
	lk.unlock();
	cvRead.notify_one();
	data = (char *)malloc(len);
	if (data != NULL)
		memcpy(data, fs + start, len);

	return data;
}

/*
 * Private Methods
 */

int STORAGE::DynamicMemoryMappedFile::getFileDescriptor(const char *fname, bool create) {
	int fd;
	int err;
	if (create) {
		err = _sopen_s(&fd, fname, _O_RDWR | _O_BINARY | _O_RANDOM | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE);
	} else {
		err = _sopen_s(&fd, fname, _O_RDWR | _O_BINARY | _O_RANDOM, _SH_DENYNO, _S_IREAD | _S_IWRITE);
	}
	if (err != 0) {
		std::ostringstream os;
		os << err;
		logEvent(ERROR, "Unable to open backing file, aborted with error " + os.str());
		shutdown(FAILURE);
	}
	return fd;
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

void STORAGE::DynamicMemoryMappedFile::grow(size_t newSize) {	// Increase the size by some amount
	growing = true;
	size_t oldMapSize = mapSize;
	size_t amt = newSize - oldMapSize;
	if (amt <= 0) {
		growing = false;
		return;
	}
	size_t test = (size_t)std::ceil(newSize * GROWTH_FACTOR);
	mapSize = test > maxSize ? maxSize : test;

#if EXTRATESTING
	std::ostringstream os;
	os << mapSize;
	logEvent(EVENT, "Growing filesystem to " + os.str());
#endif

	int fd = getFileDescriptor(backingFilename);
	ftruncate(fd, mapSize);
	fs = (char*)mmap((void*)NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	_close(fd);
	if (fs == MAP_FAILED) {
		// Uhoh...
		logEvent(ERROR, "Could not remap backing file after growing");
		shutdown(FAILURE);
	}

	growing = false;
}
