#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_
#pragma once

#include "DynamicMemoryMappedFile.h"
#include "Logging.h"

#include <limits>

/*
 * The filesystem manipulates the raw memory mapped file in order 
 * to manage the contents for the user.
 *

Filesystem structure:
[File Directory] --  Preallocated to allow for a maximum of 2^32 files.
	[Num Files]
	{ File Positions
		...
		[Position]
		...
[Files]
	...
	{ File
		{ File header
			[Name]
			[Size]
		}
		[File data]
	}
	...
*/

namespace STORAGE {
	static const unsigned short MAXFILES = std::numeric_limits<unsigned short>::max();

	struct FileDirectory {
		static const unsigned int SIZE = sizeof(unsigned short) + sizeof(off_t) * MAXFILES;
		unsigned short numFiles;
		off_t positions[MAXFILES];
		FileDirectory(unsigned short num) : numFiles(num) {
			for (int i = 0; i < MAXFILES; ++i) {
				positions[i] = -1;
			}
		}
	};

	class Filesystem {
	public:
		Filesystem(const char* fname);
		void shutdown(int code = SUCCESS);

	private:
		DynamicMemoryMappedFile file;
		FileDirectory *dir;
		void writeFileDirectory(FileDirectory *);
		FileDirectory *readFileDirectory();
	};
}

#endif
