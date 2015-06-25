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
	if (!fileExists(backingFilename)) {
		createInitial = true;
	}
	int fd = getFileDescriptor(backingFilename);

	// Map a small size, update once the actual size is determined from header
	mapSize = INITIAL_PAGES * PAGE_SIZE;

	fs = (char*)mmap((void*)NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fs == MAP_FAILED) {
		// Uhoh...
		// TODO: Add logging...
		_close(fd);
		shutdown(FAILURE);
	}

	if (createInitial) {
		logEvent(EVENT, "Creating initial file structure");
		ftruncate(fd, mapSize);
		writeHeader();
	}
	else {
		// Read header, perform sanity check, remap.
		char *header = readHeader();
		if (!sanityCheck(header)) {
			// Uhoh...
			shutdown(FAILURE);
		}
		// Extract version and size.
		short recordedVersion;
		memcpy(&recordedVersion, header + sizeof(SANITY), sizeof(VERSION));
		// If the version differs than there could be some compatibility issues...
		if (recordedVersion != VERSION) {
			// What do?
			logEvent(ERROR, "Version mismatch!");
			shutdown(FAILURE);
		}

		memcpy(&mapSize, header + sizeof(SANITY) + sizeof(VERSION), sizeof(mapSize));

		// mmap over the previous region
		fs = (char*)mmap(fs, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

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
	writeHeader();
	munmap(fs, mapSize);
	return code;
}

int STORAGE::DynamicMemoryMappedFile::raw_write(const char *data, size_t len, off_t pos) {
	// If we are trying to write beyond the end of the file, we must grow.
	size_t start = pos + HEADER_SIZE;
	size_t end = start + len;
	if (end > mapSize - 1) {
		grow(end - mapSize - 1);
	}
	memcpy(fs + start, data, len);
	return 0;
}

char *STORAGE::DynamicMemoryMappedFile::raw_read(off_t pos, size_t len) {
	if (pos + len > mapSize - 1) {
		// Crash gently...
		shutdown(FAILURE);
	}
	char *data = (char *)malloc(len);
	memcpy(data, fs + pos, len);
	return data;
}

/*
 * Private Methods
 */

int STORAGE::DynamicMemoryMappedFile::getFileDescriptor(const char *fname) {
	int fd;
	int err = _sopen_s(&fd, fname, _O_RDWR | _O_BINARY | _O_RANDOM | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE);
	if (err != 0) {
		// Todo: Add Logging
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
	char *header = raw_read(0, HEADER_SIZE);
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
	if (needed > PAGE_SIZE) {
		int p = (int)ceil((double)needed / (double)PAGE_SIZE);
		amt = p * PAGE_SIZE;
	}

	// Increase the size by some size
	static const unsigned int maxSize = (int)pow(2, 31);
	mapSize = mapSize + amt;// > maxSize ? maxSize : mapSize + amt;
	int fd = getFileDescriptor(backingFilename);
	ftruncate(fd, mapSize);
	fs = (char*)mmap(fs, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	_close(fd);
	if (fs == MAP_FAILED) {
		// Uhoh...
		shutdown(FAILURE);
	}
}