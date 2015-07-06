#include "DynamicMemoryMappedFile.h"
#include "Logging.h"

#if defined(_WIN32) || defined(_WIN64)
int ftruncate(int fd, size_t len) {
	return _chsize_s(fd, len);
}
#endif

/*
 * Constructor!
 */
STORAGE::DynamicMemoryMappedFile::DynamicMemoryMappedFile(const char* fname) : backingFilename(fname) {
	logEvent(EVENT, "Using backing file '" + std::string(fname, strlen(fname)) + "'");
	// If the backing file does not exist, we need to create it
	bool createInitial = false;

	int fd;

	if (!fileExists(backingFilename)) {
		isNewFile = true;
		fd = getFileDescriptor(backingFilename, true);
		createInitial = true;
		mapSize = INITIAL_PAGES * PAGE_SIZE;
	} else {
		// Read file size from the host filesystem
		isNewFile = false;
		fd = getFileDescriptor(backingFilename, false);
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
		char *header = readHeader();
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

#ifdef LOGDEBUGGING
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
		free(header);
	}

	// We don't actually need the file descriptor any longer
	_close(fd);
}

/*
 * Public Methods
 */

int STORAGE::DynamicMemoryMappedFile::shutdown(const int code = SUCCESS) {
	std::stringstream codeStr;
	codeStr << code;
	logEvent(EVENT, "Shutting down with code " + codeStr.str());
	if (code != SUCCESS) {
		logEvent(ERROR, "Shutdown failure");
		exit(code);
	}
	writeHeader();
	munmap(fs, mapSize);
	return code;
}

int STORAGE::DynamicMemoryMappedFile::raw_write(const char *data, size_t len, off_t pos) {
	// We are writing to the file.  It is not new anymore!
	if (isNewFile) {
		isNewFile = false;
	}

	// If we are trying to write beyond the end of the file, we must grow.
	size_t start = pos + HEADER_SIZE;
	size_t end = start + len;

	// Need to ensure that if one thread grows the file, other threads wait
	growthLock.lock();
	if (end > mapSize - 1) {
		grow(end - mapSize - 1);
	}
	growthLock.unlock();

	memcpy(fs + start, data, len);
	return 0;
}

char *STORAGE::DynamicMemoryMappedFile::raw_read(off_t pos, size_t len, off_t off) {
	size_t start = pos + off;
	size_t end = start + len;
	if (end > mapSize - 1) {
		// Crash gently...
		logEvent(ERROR, "Attempted to read beyond the end of the filesystem!");
		shutdown(FAILURE);
	}
	char *data = (char *)malloc(len);
	memcpy(data, fs + start, len);
	return data;
}

/*
 * Private Methods
 */

int STORAGE::DynamicMemoryMappedFile::getFileDescriptor(const char *fname, bool create = true) {
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
	memcpy(fs, SANITY, sizeof(SANITY));
	memcpy(fs + sizeof(SANITY), reinterpret_cast<char*>(&VERSION), sizeof(VERSION));
	memcpy(fs + sizeof(SANITY) + sizeof(VERSION), reinterpret_cast<char*>(&mapSize), sizeof(mapSize));
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

void STORAGE::DynamicMemoryMappedFile::grow(size_t needed = PAGE_SIZE) {
	// Calculate the number of pages needed to grow by
	int amt = PAGE_SIZE;
	needed = (size_t)ceil((double)needed*OVERAGE_FACTOR);
	if (needed > PAGE_SIZE) {
		int p = (int)ceil((double)needed / (double)PAGE_SIZE);
		amt = p * PAGE_SIZE;
	}

	// Increase the size by some size
	static const unsigned int maxSize = (unsigned int)pow(2, 32) - 1;
	mapSize = mapSize + amt > maxSize ? maxSize : mapSize + amt;
#ifdef LOGDEBUGGING
	std::ostringstream os;
	os << mapSize;
	logEvent(EVENT, "Growing filesystem to " + os.str());
#endif
	int fd = getFileDescriptor(backingFilename);
	ftruncate(fd, mapSize);
	fs = (char*)mmap(fs, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	_close(fd);
	if (fs == MAP_FAILED) {
		// Uhoh...
		logEvent(ERROR, "Could not remap backing file after growing");
		shutdown(FAILURE);
	}
}
