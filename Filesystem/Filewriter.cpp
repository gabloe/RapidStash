#include "Filewriter.h"
#include "FilesystemCommon.h"
#include "Filesystem.h"

/*
*  File writer utility class
*/
STORAGE::IO::Writer::Writer(STORAGE::Filesystem *fs_, File file_) : FileIO(fs_, file_) {}

void STORAGE::IO::Writer::write(const char *data, FileSize size) {
	TimePoint start;
	if (timingEnabled) {
		start = Clock::now();
	}

	FilePosition oldLoc = fs->dir->files[file];

	// If there is not enough excess spacel available, we must create a new file for this write
	// This generates garbage that may eventually need to be cleaned up.
	// OR if MVCC is enabled
	if (size + position > fs->dir->headers[file].virtualSize || fs->isMVCCEnabled()) {
		FilePosition newLoc = fs->relocateHeader(file, size + position);
		
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
	position += size;

	lastHeader = fs->dir->headers[file];

	if (timingEnabled) {
		TimeSpan time_span = std::chrono::duration_cast<TimeSpan>(Clock::now() - start);
		writeTime.store(writeTime.load() + time_span.count());
	}
}
