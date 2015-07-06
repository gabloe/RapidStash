#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_
#pragma once

#include "DynamicMemoryMappedFile.h"
#include "Logging.h"
#include <limits>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>

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
	static std::mutex dirLock; // If we modify anything in the file directory, it must be atomic.
	static const unsigned short MAXFILES = std::numeric_limits<unsigned short>::max();

	// Data to initially write to file location.  Used to reclaim files that get created but never written.
	static const char FilePlaceholder[] = { 0xd,0xe,0xa,0xd,0xc,0x0,0xd,0xe };

	struct FileMeta {
		static const unsigned int MAXNAMELEN = 32;
		static const unsigned int SIZE = MAXNAMELEN + 3 * sizeof(size_t) + sizeof(off_t) + sizeof(bool) + sizeof(unsigned short);
		size_t nameSize;
		char name[MAXNAMELEN];
		size_t size;
		size_t virtualSize;
		off_t position;
		bool lock;
		unsigned short numLocks; // The number of threads trying to lock this file for writing.
		FileMeta() : nameSize(0), size(0), virtualSize(0), position(0), lock(false) {}
		FileMeta(FileMeta &other) {
			nameSize = other.nameSize;
			memcpy(name, other.name, nameSize);
			size = other.size;
			virtualSize = other.virtualSize;
			position = other.position;
		}
	};

	struct File {
		unsigned short index;
		File(unsigned short index_) : index(index_) {}
	};

	struct FileDirectory {
		static const size_t MINALLOCATION = 512;  // Pre-Allocate 512 bytes per file.
		static const unsigned int SIZE	=	2 * sizeof(unsigned short) + 
											sizeof(size_t) + 
											FileMeta::SIZE * MAXFILES;
		unsigned short numFiles;
		unsigned short firstFree;
		off_t nextRawSpot;
		FileMeta files[MAXFILES];
		FileDirectory() : numFiles(0), firstFree(0), nextRawSpot(SIZE) {
			// Not really necessary, but just for cleanliness
			memset(files, 0, sizeof(FileMeta) * MAXFILES);
		}
		unsigned short insert(std::string name, unsigned int size = MINALLOCATION) {
			unsigned short spot = firstFree;
			off_t location = nextRawSpot;
			nextRawSpot += size;
			firstFree++;
			size_t nameSize = name.size();
			FileMeta meta;
			memcpy(&meta.nameSize, &nameSize, sizeof(size_t));
			memcpy(meta.name, name.c_str(), name.size());
			memcpy(&meta.position, &location, sizeof(size_t));
			files[spot] = meta;
			return spot;
		}
	};

	class Filesystem {
	public:
		Filesystem(const char* fname);
		void shutdown(int code = SUCCESS);
		File select(std::string);
		File createNewFile(std::string);
		void lock(File);
		void unlock(File);

	protected:
		// TODO: Need a way to easily iterate over file metadata.
		class FileIterator {
			FileIterator(FileIterator &other) {
			
			}

			FileIterator(unsigned short pos) {

			}

			FileIterator &operator++() {

			}

		};

	private:
		DynamicMemoryMappedFile file;
		FileDirectory *dir;
		void writeFileDirectory(FileDirectory *);
		FileDirectory *readFileDirectory();

		// For quick lookups, map filenames to spot in meta table.
		std::map<std::string, unsigned short> lookup;
	};
}

#endif
