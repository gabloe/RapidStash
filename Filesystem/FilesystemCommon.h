#ifndef _FILESYSTEMCOMMON_H_
#define _FILESYSTEMCOMMON_H_
#pragma once

#include "RapidStashCommon.h"

#include <array>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace STORAGE {
	// Atomic data.  Should avoid data races.
	static std::thread::id nobody;											// Reset for lock ownership
																			// A file is just am index into an internal array.
	static std::mutex dirLock;		// If we modify anything in the file directory, it must be atomic.
	static std::mutex insertGuard;	// Protect file directory info during inserts

	static const size_t MAXFILES = 2 << 19; // 1MB entries at 8 bytes per entry == 8MB file directory

	// Lock type are either exclusive for both reads and writes (WRITE lock) or
	// nonexclusive for reads only (READ lock).
	enum LockType {
		READLOCK,
		WRITELOCK
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
		static const size_t SIZE = MAXNAMELEN + sizeof(FilePosition) + sizeof(FileSize) + sizeof(FileVersion) + sizeof(std::chrono::milliseconds);

		// Data
		char name[MAXNAMELEN];				// The file name
		FilePosition next;					// Used in the free list and in MVCC
		FileSize size;						// The number of bytes actually used for the file
		FileVersion version;				// The version of this file for MVCC
		std::chrono::milliseconds timestamp;// The timestamp of last edit
	};

	static std::ostream& operator<<(std::ostream& out, const FileHeader &obj) {
		out << "Name: " << obj.name << "\nSize: " << obj.size << "\nNext: " << obj.next << "\nVersion: " << obj.version << "\nTimestamp: " << obj.timestamp.count();
		return out;
	}

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
