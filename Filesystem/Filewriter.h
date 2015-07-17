#ifndef _FILEWRITER_H_
#define _FILEWRITER_H_
#pragma once

#include "RapidStashCommon.h"
#include "FilesystemCommon.h"
#include "FileIOCommon.h"

namespace STORAGE{
	class Filesystem;	// Forward declare
	namespace IO {

		/*
		*  Writer class.
		*  Gives the user access to write to a specific file.  The user must perform all locking/unlocking if necessary
		*/
		class Writer {
		public:
			Writer(STORAGE::Filesystem *fs_, File file_) : fs(fs_), file(file_), position(0) {}
			void seek(off_t, StartLocation);
			void write(const char *, FileSize);
			FilePosition tell();

		private:
			STORAGE::Filesystem *fs;
			File file;
			FilePosition position;
		};
	}
}

#endif
