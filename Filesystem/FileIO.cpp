#include "FileIOCommon.h"
#include "FilesystemCommon.h"
#include "Filesystem.h"

FilePosition STORAGE::IO::FileIO::tell() {
	return position;
}

void STORAGE::IO::FileIO::seek(off_t pos, StartLocation start) {
	FilePosition &loc = fs->dir->files[file];
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
	else if (start == CURSOR) {
		if (position + pos > len || position + pos < 0) {
			throw SeekOutOfBoundsException();
		}
		position += pos;
	}
}