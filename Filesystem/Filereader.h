#ifndef _FILEREADER_H_
#define _FILEREADER_H_
#pragma once

#include "RapidStashCommon.h"
#include "FilesystemCommon.h"

namespace STORAGE {
	class Filesystem;

	/*
	*  Reader class.
	*  Gives the user access to read a specific file.  The user must perform all locking/unlocking if necessary
	*/
	class Reader {
	public:
		Reader(Filesystem *fs_, File file_) : fs(fs_), file(file_), position(0) {}
		void seek(off_t, StartLocation);
		char *read(FileSize);
		char *read();
		FilePosition tell();

	private:
		Filesystem *fs;
		File file;
		FilePosition position;
	};
}

#endif
