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
	static const unsigned short MAXFILES = std::numeric_limits<unsigned short>::max();

	// Data to initially write to file location.  Used to reclaim files that get created but never written.
	static const char FilePlaceholder[] = { 0xd,0xe,0xa,0xd,0xb,0xe,0xe,0xf };

	struct File {
		static const unsigned int MAXNAMELEN = 32;
		char name[MAXNAMELEN];  // Up to 32 bytes for filenames (256-bit)
		size_t size;
		size_t virtualSize;
		off_t location;
		char *data;
		File() : size(0), virtualSize(0), location(0) {}
		File(char *rawBuffer, off_t loc) : location(loc) {
			memcpy(name, rawBuffer, MAXNAMELEN);
			memcpy(&size, rawBuffer + MAXNAMELEN, sizeof(size_t));
			memcpy(&virtualSize, rawBuffer + MAXNAMELEN + sizeof(size_t), sizeof(size_t));
			memcpy(data, rawBuffer + MAXNAMELEN + sizeof(size_t) * 2, size);
		}
		size_t getRawSize() {
			return sizeof(size_t) * 2 + MAXNAMELEN + size;
		}
		char *getRawBuffer() {
			char *buffer = (char*)malloc(getRawSize());
			memcpy(buffer, name, MAXNAMELEN);
			memcpy(buffer + MAXNAMELEN, reinterpret_cast<char*>(&size), sizeof(size_t));
			memcpy(buffer + MAXNAMELEN + sizeof(size_t), reinterpret_cast<char*>(&virtualSize), sizeof(size_t));
			memcpy(buffer + MAXNAMELEN + sizeof(size_t) * 2, data, size);
			return buffer;
		}
	};

	struct FileMeta {
		static const unsigned int SIZE = File::MAXNAMELEN + 2 * sizeof(size_t);
		size_t nameSize;
		char name[File::MAXNAMELEN];
		size_t position;
	};

	struct FileDirectory {
		static const size_t MINALLOCATION = 512;  // Pre-Allocate 512 bytes per file.
		static const unsigned int SIZE	=	2 * sizeof(unsigned short) + 
											sizeof(size_t) + 
											FileMeta::SIZE * MAXFILES;
		unsigned short numFiles;
		unsigned short firstFree;
		size_t nextRawSpot;
		FileMeta files[MAXFILES];
		FileDirectory() : numFiles(0), firstFree(0), nextRawSpot(SIZE) {
			for (int i = 0; i < MAXFILES; ++i) {
				memset(&files[i], 0, FileMeta::SIZE);
			}
		}
		off_t insert(std::string name, unsigned int size = MINALLOCATION) {
			unsigned short spot = firstFree;
			size_t location = nextRawSpot;
			nextRawSpot += size;
			firstFree++;
			size_t nameSize = name.size();
			memcpy(&files[spot].nameSize, &nameSize, sizeof(size_t));
			memcpy(files[spot].name, name.c_str(), name.size());
			memcpy(&files[spot].position, &location, sizeof(size_t));
			return location;
		}
	};

	class Filesystem {
	public:
		Filesystem(const char* fname);
		void shutdown(int code = SUCCESS);
		File *createNewFile(std::string);

	private:
		DynamicMemoryMappedFile file;
		FileDirectory *dir;
		void writeFileDirectory(FileDirectory *);
		FileDirectory *readFileDirectory();
	};
}

#endif
