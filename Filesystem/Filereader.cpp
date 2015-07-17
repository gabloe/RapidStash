#include "Filereader.h"
#include "FilesystemCommon.h"
#include "Filesystem.h"

/*
*  File reader utility class
*/
FilePosition STORAGE::IO::Reader::tell() {
	return position;
}

void STORAGE::IO::Reader::seek(off_t pos, StartLocation start) {
	FilePosition &loc = fs->dir->files[file];
	FileSize &len = fs->dir->headers[file].size;

	if (start == BEGIN) {
		if (pos > len || pos < 0) {
			throw SeekOutOfBoundsException();
		}
		position = loc + pos;
	}
	else if (start == END) {
		if (len + pos > len || len + pos < 0) {
			throw SeekOutOfBoundsException();
		}
		position = loc + len + pos;
	}
}

char *STORAGE::IO::Reader::read() {
	FileSize &size = fs->dir->headers[file].size;

	char *buffer = NULL;
	try {
		buffer = read(size);
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

char *STORAGE::IO::Reader::read(FileSize amt) {
	std::chrono::time_point<std::chrono::steady_clock> start;
	if (timingEnabled) {
		start = std::chrono::high_resolution_clock::now();
	}

	STORAGE::FileHeader header = fs->dir->headers[file];
	FilePosition loc;
	FileSize size;

	// If we are using MVCC and the file is locked, read an old version.
	if (MVCC && fs->dir->locks[file].lock && header.version > 0 && fs->dir->locks[file].tid != std::this_thread::get_id()) {
		loc = header.next;
		header = fs->readHeader(header.next);
	}
	else {
		loc = fs->dir->files[file];
	}
	size = header.size;

	// We don't want to be able to read beyond the last byte of the file.
	if (position + amt > size) {
		throw ReadOutOfBoundsException();
	}

	char *data = fs->file.raw_read(loc + position + STORAGE::FileHeader::SIZE, amt);

	bytesRead += amt + STORAGE::FileHeader::SIZE;
	numReads++;

	if (timingEnabled) {
		auto end = std::chrono::high_resolution_clock::now();
		auto turnaround = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		readTime.store(readTime.load() + turnaround.count());
	}
	return data;
}
