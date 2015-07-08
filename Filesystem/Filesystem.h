#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_
#pragma once

#include "DynamicMemoryMappedFile.h"
#include "Logging.h"

#include <cstring>

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
	{ Files
		...
		[Name]
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

	static std::thread::id nobody;  // Reset for lock ownership

	// A file is just am index into an internal array.
	typedef unsigned short File;
	static std::mutex dirLock; // If we modify anything in the file directory, it must be atomic.

	static const unsigned short MAXFILES = (2 << 15) - 1; // std::numeric_limits<unsigned short>::max();

	// Data to initially write to file location.  Used to reclaim files that get created but never written.
	static const char FilePlaceholder[] = { 0xd,0xe,0xa,0xd,0xc,0x0,0xd,0xe };

	struct FileMeta {
		static const char MAXNAMELEN = 32;
		static const unsigned int SIZE = MAXNAMELEN + 4 * sizeof(size_t);
		
		char nameSize;				// The number of characters for the file name
		char name[MAXNAMELEN];			// The file name
		size_t size;					// The number of bytes actually used for the file
		size_t virtualSize;				// The total number of bytes allocated for the file
		size_t position;				// The position of the file on disk
		bool lock;						// The exclusive lock status of the file
		std::condition_variable cond;	// Condition variable to wait on
		std::thread::id tid;			// The owner of the lock
		int writers;					// The number of threads writing
		int readers;					// The number of threads reading

		FileMeta() : nameSize(0), size(0), virtualSize(0), position(0), lock(false), readers(0), writers(0) {}
	};

	struct FileDirectory {
		// Data
		File numFiles;
		File firstFree;
		size_t nextRawSpot;
		FileMeta files[MAXFILES];

		// Methods
		FileDirectory() : numFiles(0), firstFree(0), nextRawSpot(SIZE) {}
		File insert(std::string name, size_t size = MINALLOCATION) {
			numFiles++;
			File spot = firstFree;
			size_t location = nextRawSpot;
			nextRawSpot += size;
			firstFree++;
			size_t nameSize = name.size();
			files[spot].lock = false;
			files[spot].size = 0;
			files[spot].virtualSize = size;
			memcpy(&files[spot].nameSize, &nameSize, sizeof(char));
			memcpy(files[spot].name, name.c_str(), name.size());
			memcpy(&files[spot].position, &location, sizeof(size_t));
			return spot;
		}

		/*
		 *  Statics
		 */
		static const size_t MINALLOCATION = 1024;  // Pre-Allocate 1024 bytes per file.
		static const unsigned int SIZE = 2 * sizeof(File) + sizeof(size_t) +
			(FileMeta::SIZE * MAXFILES);
	};

	enum LockType {
		READ,
		WRITE
	};

	class Filesystem {
	public:
		friend class Writer;
		friend class Reader;

		Filesystem(const char* fname);
		void shutdown(int code = SUCCESS);
		File select(std::string);
		File createNewFile(std::string);
		void lock(File, LockType);
		void unlock(File, LockType);
		size_t getSize(File);
		Writer getWriter(File);
		Reader getReader(File);

	private:
		DynamicMemoryMappedFile file;
		void writeFileDirectory(FileDirectory *);
		FileDirectory *readFileDirectory();
		FileDirectory *dir;

		// For quick lookups, map filenames to spot in meta table.
		std::map<std::string, unsigned short> lookup;
	};

	/*
	 *  File reader/writer classes
	 */

	enum StartLocation {
		BEGIN,
		END
	};

	class Reader {
	public:
		Reader(Filesystem *fs_, File file_) : fs(fs_), file(file_), position(0) {}
		void seek(off_t, StartLocation);
		char *read(size_t);
		char *read();
		size_t tell();

	private:
		Filesystem *fs;
		File file;
		size_t position;
	};


	class Writer {
	public:
		Writer(Filesystem *fs_, File file_) : fs(fs_), file(file_), position(0) {}
		void seek(off_t, StartLocation);
		void write(const char *, size_t);
		size_t tell();

	private:
		Filesystem *fs;
		File file;
		size_t position;
	};

	class SeekOutOfBoundsException : public std::exception {
		virtual const char* what() const throw(){
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
