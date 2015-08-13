/*
 *  Filesystem.h
 *  Enables a basic Filesystem to be managed on top of a managed memory mapped file
 *
 *  Written by: Gabriel J. Loewen
 */

#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_
#pragma once

#include "MMAPFile.h"
#include "Logging.h"
#include "Filewriter.h"
#include "Filereader.h"
#include "FileIOCommon.h"

#include <cstring>
#include <array>
#include <limits>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

/*
 * The filesystem manipulates the raw memory mapped file in order 
 * to manage the contents for the user.
 *

Filesystem structure:
[File Directory] --  Preallocated to allow for a maximum of 2^16 files.
	[Num Files]
	[Next Spot]
	{ Files
		...
		[Raw Position]
		...
	}
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
	/*
	 *  Filesystem class
	 *	Manages the filesystem on top of a memory mapped file.  Provides RAII style concurrency control
	 *  for per-file locking.  Locking is left entirely up to the user, but unlock file access will most
	 *  likely result in unpredictable behavior.  A random access reader and writer class is provided.
	 */
	class Filesystem {
		friend class IO::Writer;
		friend class IO::Reader;
		friend class IO::FileIO;

	public:
		Filesystem(const char* fname);
		~Filesystem() { free(dir); }
		void shutdown(int code = SUCCESS);
		File &select(std::string);
		void lock(File, IO::LockType);
		void unlock(File, IO::LockType);
		FileHeader getHeader(File);
		IO::Writer getWriter(File);
		IO::Reader getReader(File);
		IO::SafeWriter getSafeWriter(File);
		IO::SafeReader getSafeReader(File);
		size_t count(CountType);
		double getThroughput(CountType);
		bool exists(std::string);
		void checkFreeList();
		void toggleMVCC();
		bool isMVCCEnabled();
		void resetStats();
		bool unlink(File);

	protected:
		DynamicMemoryMappedFile file;
		void writeFileDirectory(FileDirectory *);
		FileDirectory *readFileDirectory();
		FileDirectory *dir;
		File insertHeader(const char *);
		FilePosition relocateHeader(File, FileSize);
		FileHeader readHeader(File);
		FileHeader readHeader(FilePosition);
		void writeHeader(File);
		void writeHeader(FileHeader, FilePosition);
		File createNewFile(std::string);
		
		// For quick lookups, map filenames to spot in meta table.
		std::map<std::string, File> lookup;

		// Toggle multiversion concurrency control
		bool MVCC;

		bool shuttingDown;
	};
}

#endif
