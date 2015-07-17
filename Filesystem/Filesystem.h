/*
 *  Filesystem.h
 *  Enables a basic Filesystem to be managed on top of a managed memory mapped file
 *
 *  Written by: Gabriel J. Loewen
 */

#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_
#pragma once

#include "DynamicMemoryMappedFile.h"
#include "Logging.h"
#include "Filewriter.h"
#include "Filereader.h"

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
	class Filesystem;
	class Writer;
    class Reader;

	/*
	 *  Filesystem class
	 *	Manages the filesystem on top of a memory mapped file.  Provides RAII style concurrency control
	 *  for per-file locking.  Locking is left entirely up to the user, but unlock file access will most
	 *  likely result in unpredictable behavior.  A random access reader and writer class is provided.
	 */
	class Filesystem {
	public:
		friend class Writer;
		friend class Reader;

		Filesystem(const char* fname);
		void shutdown(int code = SUCCESS);
		File select(std::string);
		void lock(File, LockType);
		void unlock(File, LockType);
		FileHeader getHeader(File);
		Writer getWriter(File);
		Reader getReader(File);
		size_t count(CountType);
		long double getThroughput(CountType);
		bool exists(std::string);
		void checkFreeList();

	private:
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
	};


	/*
	*  Exceptions!
	*/
	class SeekOutOfBoundsException : public std::exception {
		virtual const char* what() const throw() {
			return "Attempted to seek beyond the end of the file or before the beginning of the file.";
		}
	};

	class ReadOutOfBoundsException : public std::exception {
		virtual const char* what() const throw() {
			return "Attempted to read beyond the end of the file or before the beginning of the file.";
		}
	};
}

#endif
