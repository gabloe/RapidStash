#include "Filewriter.h"
#include "FilesystemCommon.h"
#include "Filesystem.h"

/*
*  File writer utility class
*/
FilePosition STORAGE::Writer::tell() {
	return position;
}

void STORAGE::Writer::seek(off_t pos, StartLocation start) {
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

void STORAGE::Writer::write(const char *data, FileSize size) {
	std::chrono::time_point<std::chrono::steady_clock> start;
	if (timingEnabled) {
		start = std::chrono::high_resolution_clock::now();
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
			char *chunk = fs->file.raw_read(oldLoc + FileHeader::SIZE, position);
			fs->file.raw_write(chunk, position, newLoc + FileHeader::SIZE);
		}

		// Write the rest of the data
		fs->file.raw_write(data, size, newLoc + position + FileHeader::SIZE);
	}
	else {
		// If we aren't using MVCC and the old file size is accommodating just update metadata in directory
		fs->dir->headers[file].size = size + position;
		fs->dir->headers[file].timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

		fs->writeHeader(file);

		// Write the data
		fs->file.raw_write(data, size, oldLoc + position + FileHeader::SIZE);
	}

	bytesWritten += size + FileHeader::SIZE;
	numWrites++;

	if (timingEnabled) {
		auto end = std::chrono::high_resolution_clock::now();
		auto turnaround = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		writeTime.store(writeTime.load() + turnaround.count());
	}
}
