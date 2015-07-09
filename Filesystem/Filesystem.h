#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_
#pragma once

#include "DynamicMemoryMappedFile.h"
#include "Logging.h"

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

	static std::thread::id nobody;									// Reset for lock ownership
	static std::atomic<size_t> bytesWritten;						// Count of number of bytes written
	static std::atomic<size_t> numWrites;							// Count of number of write operations
	static std::chrono::high_resolution_clock::time_point startTime;// Start time of first write
	static std::atomic<bool> timeStarted;							// Trigger for start time calculation

	// A file is just am index into an internal array.
	static std::mutex dirLock; // If we modify anything in the file directory, it must be atomic.

	static const unsigned short MAXFILES = std::numeric_limits<unsigned short>::max();

	// Data to initially write to file location.  Used to reclaim files that get created but never written.
	static const char FilePlaceholder[] = { 0xd,0xe,0xa,0xd };

	struct FileMeta {
		static const byte MAXNAMELEN = 32;
		static const size_t SIZE = /*sizeof(size_t) + */MAXNAMELEN + (2 * sizeof(FileSize)) + sizeof(FilePosition);
		
		// Stored data
		//size_t nameSize;					// The number of characters for the file name
		char name[MAXNAMELEN];			// The file name
		FileSize size;					// The number of bytes actually used for the file
		FileSize virtualSize;			// The total number of bytes allocated for the file
		FilePosition position;			// The position of the file on disk
		//------------------------------------------------------------------------------------
		bool lock;						// The exclusive lock status of the file
		std::condition_variable cond;	// Condition variable to wait on
		std::thread::id tid;			// The owner of the lock
		int writers;					// The number of threads writing
		int readers;					// The number of threads reading

		FileMeta() : /*nameSize(0),*/ size(0), virtualSize(0), position(0), lock(false), readers(0), writers(0) {}
	};

	struct FileDirectory {
		// Data
		File numFiles;
		File firstFree;
		FilePosition nextRawSpot;
		std::array<FileMeta, MAXFILES> files;

		// Methods
		FileDirectory() : numFiles(0), firstFree(0), nextRawSpot(SIZE) {}
		File insert(const char *name, FileSize size = MINALLOCATION) {
			numFiles++;
			File spot = firstFree;
			FilePosition location = nextRawSpot;
			nextRawSpot += size;
			firstFree++;
			files[spot].lock = false;
			files[spot].size = 0;
			files[spot].virtualSize = size;
			memcpy(files[spot].name, name, strlen(name));
			memcpy(&files[spot].position, &location, sizeof(FilePosition));
			return spot;
		}

		/*
		 *  Statics
		 */
		static const FileSize MINALLOCATION = 256;  // Pre-Allocate 256 bytes per file.
		static const size_t SIZE = (2 * sizeof(File)) + sizeof(FilePosition) + (FileMeta::SIZE * MAXFILES);
	};

	enum LockType {
		READ,
		WRITE
	};

	enum CountType {
		BYTES,
		WRITES
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
		FileSize getSize(File);
		Writer getWriter(File);
		Reader getReader(File);
		size_t count(CountType);
		double getTurnaround();

	private:
		DynamicMemoryMappedFile file;
		void writeFileDirectory(FileDirectory *);
		FileDirectory *readFileDirectory();
		FileDirectory *dir;

		// For quick lookups, map filenames to spot in meta table.
		std::map<std::string, File> lookup;
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
		char *read(FileSize);
		char *read();
		FilePosition tell();

	private:
		Filesystem *fs;
		File file;
		FilePosition position;
	};


	class Writer {
	public:
		Writer(Filesystem *fs_, File file_) : fs(fs_), file(file_), position(0) {}
		void seek(off_t, StartLocation);
		void write(const char *, FileSize);
		FilePosition tell();

	private:
		Filesystem *fs;
		File file;
		FilePosition position;
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
