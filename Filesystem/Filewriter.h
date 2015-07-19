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
		class Writer : public FileIO {
		public:
			Writer(Filesystem *, File);
			void write(const char *, FileSize);
		};
	}
}

#endif
