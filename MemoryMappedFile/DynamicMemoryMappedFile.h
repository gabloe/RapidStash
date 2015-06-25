#ifndef _MEMORY_MAPPED_FILE_
#define _MEMORY_MAPPED_FILE_
#pragma once

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
#include "common.h"
#include <iostream>
#include <string>

#define HEADER_SIZE sizeof(short) + sizeof(unsigned int) + 8
static short VERSION = 1;
static char SANITY[] = { 'r','s' };

namespace STORAGE {
	class DynamicMemoryMappedFile {
		// Constants for how the filesystem will grow.  Should grow to some page boundary.
		// TODO: maybe get the actual page size from the system?
		static const int PAGE_SIZE = 4096; // 4k pages
		static const size_t INITIAL_PAGES = 32; // Start with 32 pages

	public:
		// Constructors
		DynamicMemoryMappedFile() = delete;  // There should not be a default constructor.
		DynamicMemoryMappedFile(const char*);

		/*
		 *Cleanup!
		 */
		int shutdown(const int);

		/*
		 * Write raw data to the filesystem.
		 */
		int raw_write(const char*, size_t, off_t);

		/*
		 * Read raw data from the filesystem.
		 */
		char *raw_read(off_t, size_t, off_t = HEADER_SIZE);

		bool isNew() {
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
		unsigned int mapSize;  // Gives us 4GB address space.

		/*
		 *Private methods
		 */
		int getFileDescriptor(const char*);
		void writeHeader();
		char *readHeader();
		bool sanityCheck(const char*);
		void grow(size_t);
	};
}

#endif