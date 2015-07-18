#include <chrono>

#include "Filereader.h"
#include "FilesystemCommon.h"
#include "Filesystem.h"

/*
*  File reader utility class
*/
int STORAGE::IO::Reader::readInt() {
	char *buf = readRaw(sizeof(int));
	int res;
	memcpy(&res, buf, sizeof(int));
	free(buf);
	return res;
}

char STORAGE::IO::Reader::readChar() {
	char *buf = readRaw(1);
	char res = buf[0];
	free(buf);
	return res;
}

std::string STORAGE::IO::Reader::readString() {
	FileSize &size = fs->dir->headers[file].size;
	return readString(size);
}

std::string STORAGE::IO::Reader::readString(FileSize amt) {
	char *buf = readRaw(amt);
	std::string res(buf, amt);
	free(buf);
	return res;
}

char *STORAGE::IO::Reader::readRaw() {
	FileSize &size = fs->dir->headers[file].size;

	char *buffer = NULL;
	try {
		buffer = readRaw(size);
	}
	catch (ReadOutOfBoundsException) {
		logEvent(ERROR, "Read out of bounds");
		// Generate bogus buffer
		buffer = (char*)malloc(size);
		if (buffer == NULL) {
			logEvent(ERROR, "Memory allocation failed.");
			return NULL;
		}
		memset(buffer, 0, size);
	}
	return buffer;
}

char *STORAGE::IO::Reader::readRaw(FileSize amt) {
	std::chrono::time_point<std::chrono::system_clock> start;
	if (timingEnabled) {
		start = std::chrono::system_clock::now();
	}

	STORAGE::FileHeader &header = fs->dir->headers[file];
	FilePosition loc;
	FileSize size;

	// If we are using MVCC and the file is locked, read an old version.
	if (MVCC && fs->dir->locks[file].lock && header.version > 0 && fs->dir->locks[file].tid != std::this_thread::get_id()) {
		loc = header.next;
		header = fs->readHeader(header.next);
	} else {
		loc = fs->dir->files[file];
	}
	size = header.size;

	// We don't want to be able to read beyond the last byte of the file.
	if (position + amt > size) {
		throw ReadOutOfBoundsException();
	}

	char *data = fs->file.raw_read(loc + STORAGE::FileHeader::SIZE + position, amt);

	bytesRead += amt;
	numReads++;
	position += amt;

	if (timingEnabled) {
		auto end = std::chrono::system_clock::now();
		auto turnaround = end - start;
		readTime.store(readTime.load() + turnaround.count());
	}
	return data;
}
