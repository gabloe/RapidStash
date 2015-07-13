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

	// Atomic data.  Should avoid data races.
	static std::thread::id nobody;											// Reset for lock ownership
	static std::atomic<size_t> bytesWritten;								// Count of number of bytes written
	static std::atomic<size_t> numWrites;									// Count of number of write operations
	static std::atomic<size_t> bytesRead;									// Count of number of bytes read
	static std::atomic<size_t> numReads;									// Count of number of read operations
	static std::chrono::high_resolution_clock::time_point startWriteTime;	// Start time of first write
	static std::chrono::high_resolution_clock::time_point startReadTime;	// Start time of first read
	static std::atomic<bool> writeTimeStarted;								// Trigger for start time calculation
	static std::atomic<bool> readTimeStarted;								// Trigger for start time calculation

	// A file is just am index into an internal array.
	static std::mutex dirLock;		// If we modify anything in the file directory, it must be atomic.
	static std::mutex insertGuard;	// Protect file directory info during inserts

	static const size_t MAXFILES = 2 << 19; // 1MB entries at 8 bytes per entry == 8MB file directory

	// Lock type are either exclusive for both reads and writes (WRITE lock) or
	// nonexclusive for reads only (READ lock).
	enum LockType {
		READ,
		WRITE
	};

	struct FileLock {		
		bool lock;						// The exclusive lock status of the file
		std::condition_variable cond;	// Condition variable to wait on
		std::thread::id tid;			// The owner of the lock
		int writers;					// The number of threads writing
		int readers;					// The number of threads reading

		FileLock() : lock(false), readers(0), writers(0) {}
	};

	struct FileHeader {
		// Statics
		static const int MAXNAMELEN = 32;	// Allow for up to 32 character long names
		static const size_t SIZE = MAXNAMELEN + sizeof(bool) + sizeof(FilePosition) + 2 * sizeof(FileSize) + sizeof(FileVersion);

		// Data
		char name[MAXNAMELEN];				// The file name
		bool temp;							// Flag for temp list membership
		FilePosition next;					// Ised in the free list and in MVCC
		FileSize size;						// The number of bytes actually used for the file
		FileSize virtualSize;				// The total number of bytes allocated for the file
		FileVersion version;				// The version of this file for MVCC
	};

	struct FileDirectory {
		// Data
		File numFiles;
		File nextSpot;
		FilePosition tempList;
		FilePosition nextRawSpot;
		std::array<FilePosition, MAXFILES> files;		// The metadata consists of where the file is located and locking info
		
		// Extra stuff
		std::array<FileLock, MAXFILES> locks;			// Per-file concurrency
		std::array<FileHeader, MAXFILES> headers;		// The headers contain the filename and file size

		// Methods
		FileDirectory() : numFiles(0), nextSpot(0), tempList(0), nextRawSpot(SIZE) {}

		/*
		 *  Statics
		 */
		static const size_t SIZE = (2 * sizeof(File)) + (2 * sizeof(FilePosition)) + (sizeof(FilePosition) * MAXFILES);
	};

	// Statistics for writes and reads
	enum CountType {
		BYTESWRITTEN,
		WRITES,
		BYTESREAD,
		READS
	};

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
		File select(std::string, size_t = 0);
		void lock(File, LockType);
		void unlock(File, LockType);
		FileHeader getHeader(File);
		Writer getWriter(File);
		Reader getReader(File);
		size_t count(CountType);
		double getWriteTurnaround();
		double getReadTurnaround();
		bool exists(std::string);
		void clearTempList();

	private:
		DynamicMemoryMappedFile file;
		void writeFileDirectory(FileDirectory *);
		FileDirectory *readFileDirectory();
		FileDirectory *dir;
		File insert(const char *, FileSize = 0, File = 0, bool = false);
		FileHeader readHeader(File);
		FileHeader readHeader(FilePosition);
		void writeHeader(File);
		void writeHeader(FileHeader, FilePosition);
		File createNewFile(std::string, size_t);

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

	/*
	 *  Reader class.
	 *  Gives the user access to read a specific file.  The user must perform all locking/unlocking if necessary
	 */
	class Reader {
	public:
		Reader(Filesystem *fs_, File file_) : fs(fs_), file(file_), position(0) {
			header = fs->getHeader(file);
		}
		void seek(off_t, StartLocation);
		char *read(FileSize);
		char *read();
		FilePosition tell();

	private:
		Filesystem *fs;
		File file;
		FilePosition position;
		FileHeader header;
	};

	/*
	*  Writer class.
	*  Gives the user access to write to a specific file.  The user must perform all locking/unlocking if necessary
	*/
	class Writer {
	public:
		Writer(Filesystem *fs_, File file_) : fs(fs_), file(file_), position(0) {
			header = fs->getHeader(file);
		}
		void seek(off_t, StartLocation);
		void write(const char *, FileSize);
		FilePosition tell();

	private:
		Filesystem *fs;
		File file;
		FilePosition position;
		FileHeader header;
	};


	/*
	 *  Exceptions!
	 */
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
