#ifndef _FILESYSTEMCOMMON_H_
#define _FILESYSTEMCOMMON_H_
#pragma once

#include "RapidStashCommon.h"

#include <array>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace STORAGE {
	// Atomic data.  Should avoid data races.
	static std::thread::id nobody;											// Reset for lock ownership
																			// A file is just am index into an internal array.
	static std::mutex dirLock;		// If we modify anything in the file directory, it must be atomic.
	static std::mutex insertGuard;	// Protect file directory info during inserts
	static std::mutex selectLock;

	static const size_t MAXFILES = 2 << 19; // 1MB entries at 8 bytes per entry == 8MB file directory

	struct FileLock {
		std::condition_variable cond;	// Condition variable to wait on
		int writers;					// The number of threads writing
		int readers;					// The number of threads reading

		FileLock() : readers(0), writers(0) {}
		FileLock(const FileLock &other) {
			writers = other.writers;
			readers = other.readers;
		}
		FileLock &operator=(const FileLock&);
	};

	inline FileLock& FileLock::operator= (const FileLock &src) {
		writers = src.writers;
		readers = src.readers;
		return *this;
	}

	struct FileHeader {
		// Statics
		static const int MAXNAMELEN = 32;	// Allow for up to 32 character long names
		static const size_t SIZE = MAXNAMELEN + sizeof(FilePosition) + 2 * sizeof(FileSize) + sizeof(FileVersion) + sizeof(std::chrono::milliseconds);

		// Data
		char name[MAXNAMELEN];				// The file name
		FilePosition next;					// Used in the free list and in MVCC
		FileSize size;						// The number of bytes actually used for the file
		FileSize virtualSize;				// The actual size available to the file
		FileVersion version;				// The version of this file for MVCC
		std::chrono::milliseconds timestamp;// The timestamp of last edit
	};

	struct FileDirectory {
		// Data
		FileIndex numFiles;
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
		static const size_t SIZE = sizeof(FileIndex) + sizeof(File) + (2 * sizeof(FilePosition)) + (sizeof(FilePosition) * MAXFILES);
	};

	// Statistics for writes and reads
	enum CountType {
		BYTESWRITTEN,
		NUMWRITES,
		BYTESREAD,
		NUMREADS,
		FILES,
		WRITETIME,
		READTIME
	};

	enum ThroughputType {
		WRITE,
		READ
	};
}

#endif
