#include <chrono>

#include "Filereader.h"
#include "FilesystemCommon.h"
#include "Filesystem.h"

/*
 *  Safe (auto-locking) File reader utility class
 */
STORAGE::IO::SafeReader::SafeReader(STORAGE::Filesystem *fs_, File file_) : Reader(fs_, file_) {}

char *STORAGE::IO::SafeReader::readRaw() {
	char *data;
	fs->lock(file, NONEXCLUSIVE);
	{
		data = Reader::readRaw();
	}
	fs->unlock(file, NONEXCLUSIVE);
	return data;
}

/*
 *  File reader utility class
 */
STORAGE::IO::Reader::Reader(STORAGE::Filesystem *fs_, File file_) : FileIO(fs_, file_) {}

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
	TimePoint start;
	if (timingEnabled) {
		start = Clock::now();
	}

	STORAGE::FileHeader &header = fs->dir->headers[file];
	FilePosition loc;
	FileSize size;

	// If we are using MVCC and the file is being written, read an old version.
	if (fs->isMVCCEnabled() && 
		fs->dir->locks[file].writers > 0 && header.version > 0) {
		loc = header.next;
		header = fs->readHeader(loc);
	} else {
		loc = fs->dir->files[file];
	}
	size = header.size;
	lastHeader = header;

	// We don't want to be able to read beyond the last byte of the file.
	if (position + amt > size) {
		throw ReadOutOfBoundsException();
	}

	char *data = fs->file.raw_read(loc + STORAGE::FileHeader::SIZE + position, amt);

	bytesRead += amt;
	numReads++;
	position += amt;

	if (timingEnabled) {
		TimeSpan time_span = std::chrono::duration_cast<TimeSpan>(Clock::now() - start);
		readTime.store(readTime.load() + time_span.count());
	}
	return data;
}
