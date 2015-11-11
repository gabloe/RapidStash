/*
*  DynamicMemoryMappedFile.h
*  Wraps memory mapped file creation with dynamic growth.  Manages when to grow the underlying file and provides
*  raw unmanaged read and write access.
*
*  Written by: Gabriel J. Loewen
*/

#include <Windows.h>

#ifdef MMAPFILE_EXPORTS
#define MMAPFILEDLL_API __declspec(dllexport) 
#else
#define MMAPFILEDLL_API __declspec(dllimport) 
#endif

#ifndef _MEMORY_MAPPED_FILE_
#define _MEMORY_MAPPED_FILE_
#pragma once

// Trying to support cross compatibility
#if defined(_WIN32) || defined(_WIN64)
#include "mman.h"
#include <io.h>
int ftruncate(int, size_t);
#else
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#endif

#include <math.h>
#include <fcntl.h>
#include <algorithm>
#include "RapidStashCommon.h"
#include <iostream>
#include <string>
#include <cstring>
#include <mutex>
#include <atomic>

#define GROWTH_FACTOR 1.05 // Grow 5% larger than requested.  This helps to prevent excessive calls to grow
static short VERSION = 1;
static char SANITY[] = { 0x0,0x0,0xd,0x1,0xe,0x5,0x0,0xf,0xd,0x0,0x0,0xd,0xa,0xd,0x5 };
#define HEADER_SIZE sizeof(VERSION) + sizeof(SANITY) + sizeof(size_t)

// Limit the overall size of the file to 4GB for compatibility reasons
static const size_t maxSize = (size_t)(std::pow(2, 32) - 1);

// Test for file existence
bool fileExists(const char*);

namespace STORAGE {
	class DynamicMemoryMappedFile {
		static const int INITIAL_SIZE = 4096; // Initial size of the map is 4k

	public:
		// Constructors
		MMAPFILEDLL_API DynamicMemoryMappedFile() = delete;  // There should not be a default constructor.
		MMAPFILEDLL_API DynamicMemoryMappedFile(const char*);

		/*
		 *Cleanup!
		 */
		MMAPFILEDLL_API int shutdown(const int = SUCCESS);

		/*
		 * Write raw data to the filesystem.
		 */
		MMAPFILEDLL_API int raw_write(const char*, size_t, size_t);

		/*
		 * Read raw data from the filesystem.
		 */
		MMAPFILEDLL_API char *raw_read(size_t, size_t, size_t = HEADER_SIZE);

		/*
		 * The file is new until it is written to for the first time
		 */
		MMAPFILEDLL_API bool isNew() {
			return isNewFile;
		}

	private:
		/*
		 * Private fields
		 */
		bool isNewFile;
		const char *backingFilename;
		char *fs;
		int numPages;
		size_t mapSize;
		std::mutex growthLock;
		HANDLE fHandle;
		intptr_t fd;

		/*
		 *Private methods
		 */
		HANDLE getFileDescriptor(const char*, bool = true);
		void writeHeader();
		char *readHeader();
		bool sanityCheck(const char*);
		void grow(size_t);
		size_t align(size_t);
	};
}

#endif
