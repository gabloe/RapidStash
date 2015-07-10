/*
 *  Filesystem.h
 *  Enables a basic Filesystem to be managed on top of a managed memory mapped file
 *
 *  Written by: Gabriel Loewen
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
	static std::mutex dirLock; // If we modify anything in the file directory, it must be atomic.
	static std::mutex insertGuard;

	static const size_t MAXFILES = 2 << 19; // 1MB entries at 8 bytes per entry == 8MB file directory

	struct FileMeta {
		static const size_t SIZE = sizeof(FilePosition);
		
		// Stored data
		FilePosition position;			// The position of the file on disk
		//------------------------------------------------------------------------------------

		bool lock;						// The exclusive lock status of the file
		std::condition_variable cond;	// Condition variable to wait on
		std::thread::id tid;			// The owner of the lock
		int writers;					// The number of threads writing
		int readers;					// The number of threads reading

		FileMeta() : position(0), lock(false), readers(0), writers(0) {}
	};

	struct FileHeader {
		// Statics
		static const int MAXNAMELEN = 32;	// Allow for up to 32 character long names
		static const size_t SIZE = MAXNAMELEN + 2 * sizeof(FileSize);

		// Data
		char name[MAXNAMELEN];				// The file name
		FileSize size;						// The number of bytes actually used for the file
		FileSize virtualSize;				// The total number of bytes allocated for the file
	};

	struct FileDirectory {
		// Data
		File numFiles;
		File firstFree;
		FilePosition nextRawSpot;
		FileMeta *files;			// The metadata consists of where the file is located and locking info
		FileHeader *headers;		// The headers contain the filename and file size

		// Methods
		FileDirectory() : numFiles(0), firstFree(0), nextRawSpot(SIZE) {
			files = new FileMeta[MAXFILES];
			headers = new FileHeader[MAXFILES];
		}
		
		~FileDirectory() {
			delete files;
			delete headers;
		}

		/*
		 *  Statics
		 */
		static const FileSize MINALLOCATION = 128;  // Pre-Allocate 128 bytes per file (plus header size)
		static const size_t SIZE = (2 * sizeof(File)) + sizeof(FilePosition) + (FileMeta::SIZE * MAXFILES);
	};

	enum LockType {
		READ,
		WRITE
	};

	enum CountType {
		BYTESWRITTEN,
		WRITES,
		BYTESREAD,
		READS
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
		FileHeader getHeader(File);
		Writer getWriter(File);
		Reader getReader(File);
		size_t count(CountType);
		double getWriteTurnaround();
		double getReadTurnaround();

	private:
		DynamicMemoryMappedFile file;
		void writeFileDirectory(FileDirectory *);
		FileDirectory *readFileDirectory();
		FileDirectory *dir;
		File insert(const char *, FileSize = FileDirectory::MINALLOCATION, File = 0, bool = false);
		FileHeader readHeader(File);
		void writeHeader(File);

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
