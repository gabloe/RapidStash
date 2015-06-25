#ifndef _MEMORY_MAPPED_FILE_
#define _MEMORY_MAPPED_FILE_
#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include "mman.h"
#include <io.h>
int ftruncate(int fd, size_t len) {
	return _chsize_s(fd, len);
}
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
short VERSION = 1;
char SANITY[] = { 'r','s' };

namespace STORAGE {
	class DynamicMemoryMappedFile {
		// Constants for how the filesystem will grow.  Should grow to some page boundary.
		// TODO: maybe get the actual page size from the system?
		static const int PAGE_SIZE = 4096; // 4k pages
		static const size_t INITIAL_PAGES = 32; // Start with 32 pages

	public:
		DynamicMemoryMappedFile(const char *fname) {
			backingFilename = fname;
			// If the backing file does not exist, we need to create it
			bool createInitial = false;
			if (!fileExists(backingFilename)) {
				createInitial = true;
			}
			int fd = getFileDescriptor(backingFilename);

			// Map a small size, update once the actual size is determined from header
			mapSize = INITIAL_PAGES * PAGE_SIZE;

			fs = (char*)mmap((void*)NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (fs == MAP_FAILED) {
				// Uhoh...
				// TODO: Add logging...
				_close(fd);
				shutdown(FAILURE);
			}
			
			if (createInitial) {
				ftruncate(fd, mapSize);
				writeHeader();
			}
			else {
				// Read header, perform sanity check, remap.
				char *header = readHeader();
				if (!sanityCheck(header)) {
					// Uhoh...
					shutdown(FAILURE);
				}
				// Extract version and size.
				short recordedVersion;
				memcpy(&recordedVersion, header + sizeof(SANITY), sizeof(VERSION));
				// If the version differs than there could be some compatibility issues...
				if (recordedVersion != VERSION) {
					// What do?
					shutdown(FAILURE);
				}

				memcpy(&mapSize, header + sizeof(SANITY) + sizeof(VERSION), sizeof(mapSize));

				// mmap over the previous region
				fs = (char*)mmap(fs, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

				// Cleanup
				free(header);
			}

			// We don't actually need the file descriptor any longer
			_close(fd);
		}

		int shutdown(const int code = SUCCESS) {
			writeHeader();
			munmap(fs, mapSize);
			return code;
		}

		/*
		 * Write raw data to the filesystem.
		 */
		int raw_write(const char *data, size_t len, off_t pos, off_t offset = HEADER_SIZE) {
			// If we are trying to write beyond the end of the file, we must grow.
			size_t start = pos + offset;
			size_t end = start + len;
			if (end > mapSize - 1) {
				grow(end - mapSize - 1);
			}
			memcpy(fs + start, data, len);
			return 0;
		}

		/*
		 * Read raw data from the filesystem.
		 */
		char *raw_read(off_t pos, size_t len) {
			if (pos + len > mapSize - 1) {
				// TODO: Logging
				// Crash gently...
				shutdown(FAILURE);
			}
			char *data = (char *)malloc(len);
			memcpy(data, fs + pos, len);
			return data;
		}

	private:
		const char *backingFilename;
		char *fs;
		int numPages;
		unsigned int mapSize;  // Gives us 4GB address space.

		int getFileDescriptor(const char *fname) {
			int fd;
			int err = _sopen_s(&fd, fname, _O_RDWR | _O_BINARY | _O_RANDOM | _O_CREAT, _SH_DENYNO, _S_IREAD | _S_IWRITE);
			if (err != 0) {
				// Todo: Add Logging
				shutdown(FAILURE);
			}
			return fd;
		}

		void writeHeader() {
			memcpy(fs, SANITY, sizeof(SANITY));
			memcpy(fs + sizeof(SANITY), reinterpret_cast<char*>(&VERSION), sizeof(VERSION));
			memcpy(fs + sizeof(SANITY) + sizeof(VERSION), reinterpret_cast<char*>(&mapSize), sizeof(mapSize));
			msync(fs, HEADER_SIZE, MS_SYNC);
		}

		char *readHeader() {
			char *header = raw_read(0, HEADER_SIZE);
			return header;
		}

		bool sanityCheck(char *header) {
			for (int i = 0; i < sizeof(SANITY); ++i) {
				if (header[i] != SANITY[i]) {
					return false;
				}
			}
			return true;
		}

		void grow(size_t needed = PAGE_SIZE) {
			// Calculate the number of pages needed to grow by
			int amt = PAGE_SIZE;
			if (needed > PAGE_SIZE) {
				int p = (int)ceil((double)needed / (double)PAGE_SIZE);
				amt = p * PAGE_SIZE;
			}

			// Increase the size by some size
			static const unsigned int maxSize = (int)pow(2, 31);
			mapSize = mapSize + amt;// > maxSize ? maxSize : mapSize + amt;
			int fd = getFileDescriptor(backingFilename);
			ftruncate(fd, mapSize);
			fs = (char*)mmap(fs, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			_close(fd);
			if (fs == MAP_FAILED) {
				// Uhoh...
				shutdown(FAILURE);
			}
		}
	};
}

#endif