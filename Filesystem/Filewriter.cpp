#include "Filewriter.h"
#include "FilesystemCommon.h"
#include "Filesystem.h"

/*
*  File writer utility class
*/
FilePosition STORAGE::IO::Writer::tell() {
	return position;
}

void STORAGE::IO::Writer::seek(off_t pos, StartLocation start) {
	FileSize &len = fs->dir->headers[file].size;

	if (start == BEGIN) {
		if (pos > len || pos < 0) {
			throw SeekOutOfBoundsException();
		}
		position = pos;
	}
	else if (start == END) {
		if (len + pos > len || len + pos < 0) {
			throw SeekOutOfBoundsException();
		}
		position = len + pos;
	}
}

void STORAGE::IO::Writer::write(const char *data, FileSize size) {
	std::chrono::time_point<std::chrono::system_clock> start;
	if (timingEnabled) {
		start = std::chrono::system_clock::now();
	}

	FilePosition oldLoc = fs->dir->files[file];
	FileSize oldSize = fs->dir->headers[file].size;

	// If there is not enough excess spacel available, we must create a new file for this write
	// This generates garbage that may eventually need to be cleaned up.
	// OR if MVCC is enabled
	if (size + position > oldSize || MVCC) {
		FilePosition newLoc = fs->relocateHeader(file, size + position);
		fs->dir->files[file] = newLoc;

		// If we are writing somewhere in the middle of the file, we have to copy over some of the beginning
		// of the old file.
		if (position > 0) {
			char *chunk = fs->file.raw_read(oldLoc + STORAGE::FileHeader::SIZE, position);
			fs->file.raw_write(chunk, position, newLoc + STORAGE::FileHeader::SIZE);
		}

		// Write the rest of the data
		fs->file.raw_write(data, size, newLoc + position + STORAGE::FileHeader::SIZE);
	}
	else {
		// If we aren't using MVCC and the old file size is accommodating just update metadata in directory
		fs->dir->headers[file].size = size + position;
		fs->dir->headers[file].timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

		fs->writeHeader(file);

		// Write the data
		fs->file.raw_write(data, size, oldLoc + position + STORAGE::FileHeader::SIZE);
	}

	bytesWritten += size + STORAGE::FileHeader::SIZE;
	numWrites++;

	if (timingEnabled) {
		auto end = std::chrono::system_clock::now();
		auto turnaround = end - start;
		writeTime.store(writeTime.load() + turnaround.count());
	}
}
