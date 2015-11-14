/*
*  Filesystem.cpp
*  Enables a basic Filesystem to be managed on top of a managed memory mapped file
*
*  Written by: Gabriel J. Loewen
*/

#include "Filesystem.h"
#include "Filereader.h"
#include "FileIOCommon.h"
#include <assert.h>
#include <future>

// Constructor
STORAGE::Filesystem::Filesystem(const char* fname) : file(fname), shuttingDown(false) {
	resetStats();
	MVCC = false;

	// Set up file directory if the backing file is new.
	if (file.isNew()) {
		logEvent(EVENT, "Backing file is new");
		dir = new FileDirectory();
		writeFileDirectory(dir);
	} else {
		logEvent(EVENT, "Backing file exists, populating lookup table");
		dir = readFileDirectory();
		logEvent(EVENT, "Number of stored files is " + toString(dir->numFiles));

		// Populate lookup table
		for (File i = 0; i < dir->numFiles; ++i) {
			dir->headers[i] = readHeader(i);
			std::string name(dir->headers[i].name);
			lookup[name] = i;
		}
	}
}

void STORAGE::Filesystem::toggleMVCC() {
	MVCC = !MVCC;
}

bool STORAGE::Filesystem::isMVCCEnabled() {
	return MVCC;
}

void STORAGE::Filesystem::resetStats() {
	IO::bytesWritten = 0;
	IO::bytesRead = 0;
	IO::numWrites = 0;
	IO::numReads = 0;
	IO::writeTime = 0;
	IO::readTime = 0;
}

// Remove a file from the filesystem
bool STORAGE::Filesystem::unlink(File f) {
	std::string thisName;
	bool merged = false;
	lock(f, IO::EXCLUSIVE);
	{
		// Save info about the file
		FilePosition pos = dir->files[f];
		FileSize size = dir->headers[f].size;
		FileSize vsize = dir->headers[f].virtualSize;
		thisName = std::string(dir->headers[f].name);

		// Overwrite the file with the last file
		FilePosition lastFilePos = dir->files[dir->numFiles - 1];
		FileHeader lastFileHeader = readHeader(lastFilePos);
		File &lastFile = lookup[std::string(lastFileHeader.name)];
		lock(lastFile, IO::EXCLUSIVE);
		{
			dir->files[f] = dir->files[lastFile];
			dir->headers[f] = dir->headers[lastFile];
			dir->locks[f] = dir->locks[lastFile];
			lookup[std::string(dir->headers[lastFile].name)] = f;
		}
		unlock(lastFile, IO::EXCLUSIVE);

		// Validate that the next header is valid.  Otherwise we cannot reclaim the space.
		FileHeader nextHeader = readHeader(pos + size + FileHeader::SIZE);
		if (strcmp(nextHeader.name, "") != 0 ||
			lookup.find(std::string(nextHeader.name, strlen(nextHeader.name))) != lookup.end()) {
			File nextFile = lookup[std::string(nextHeader.name)];
			FileHeader &nextFileHeader = dir->headers[nextFile];
			auto reader = getReader(nextFile);
			auto writer = getWriter(nextFile);
			lock(nextFile, IO::EXCLUSIVE);
			{
				char *buf = reader.readRaw();
				dir->files[nextFile] = pos;	// Update the file position
				dir->headers[nextFile].virtualSize += vsize + FileHeader::SIZE;
				writer.write(buf, nextFileHeader.size);
				writeHeader(nextFile);
			}
			unlock(nextFile, IO::EXCLUSIVE);
			merged = true;
		}
	}
	unlock(f, IO::EXCLUSIVE);

	// Remove the lookup info and fix the directory metadata
	{
		std::unique_lock<std::mutex> lk(dirLock);
		lookup.erase(thisName);
		dir->numFiles--;
		dir->nextSpot--;
	}

	return merged;
}

STORAGE::FileHeader STORAGE::Filesystem::readHeader(FilePosition pos) {
	STORAGE::FileHeader header;
	char *buffer = file.raw_read(pos, FileHeader::SIZE);

	size_t offset = 0;
	memcpy(header.name, buffer + offset, FileHeader::MAXNAMELEN);
	offset += FileHeader::MAXNAMELEN;
	memcpy(&header.next, buffer + offset, sizeof(FilePosition));
	offset += sizeof(FilePosition);
	memcpy(&header.size, buffer + offset, sizeof(FileSize));
	offset += sizeof(FileSize);
	memcpy(&header.virtualSize, buffer + offset, sizeof(FileSize));
	offset += sizeof(FileSize);
	memcpy(&header.version, buffer + offset, sizeof(FileVersion));
	offset += sizeof(FileVersion);
	memcpy(&header.timestamp, buffer + offset, sizeof(std::chrono::milliseconds));
	free(buffer);

	return header;
}

// Read the header for a particular file from disk
STORAGE::FileHeader STORAGE::Filesystem::readHeader(File f) {
	FilePosition &pos = dir->files[f];
	return readHeader(pos);
}

void STORAGE::Filesystem::writeHeader(FileHeader header, FilePosition pos) {
	char *buffer = (char*)malloc(FileHeader::SIZE);
	if (buffer == NULL) {
		logEvent(ERROR, "Memory allocation failed.");
		return;
	}
	size_t offset = 0;
	memcpy(buffer + offset, header.name, FileHeader::MAXNAMELEN);
	offset += FileHeader::MAXNAMELEN;
	memcpy(buffer + offset, &header.next, sizeof(FilePosition));
	offset += sizeof(FilePosition);
	memcpy(buffer + offset, reinterpret_cast<char*>(&header.size), sizeof(FileSize));
	offset += sizeof(FileSize);
	memcpy(buffer + offset, reinterpret_cast<char*>(&header.virtualSize), sizeof(FileSize));
	offset += sizeof(FileSize);
	memcpy(buffer + offset, reinterpret_cast<char*>(&header.version), sizeof(FileVersion));
	offset += sizeof(FileVersion);
	memcpy(buffer + offset, reinterpret_cast<char*>(&header.timestamp), sizeof(std::chrono::milliseconds));
	file.raw_write(buffer, FileHeader::SIZE, pos);
	free(buffer);
}

// Write a files header to disk
void STORAGE::Filesystem::writeHeader(File f) {
	FilePosition &pos = dir->files[f];
	FileHeader &header = dir->headers[f];
	writeHeader(header, pos);
}

// Create a new file
File STORAGE::Filesystem::createNewFile(std::string fname) {
	logEvent(EVENT, "Creating file: " + fname);
	if (fname.size() > FileHeader::MAXNAMELEN) {
		fname = std::string(fname, FileHeader::MAXNAMELEN);
	}

	return insertHeader(fname.c_str());
}

FilePosition STORAGE::Filesystem::relocateHeader(File oldFile, FileSize size) {
	FilePosition newPosition;
	{
		std::lock_guard<std::mutex> lk(insertGuard); // Avoid potential data races here

		// Calculate new position of file
		FileSize totalSize = size + FileHeader::SIZE;
		newPosition = dir->nextRawSpot;
		dir->nextRawSpot += totalSize;

		FilePosition oldPosition = dir->files[oldFile];
		FileHeader newHeader;

		// Copy data into the new header
		strcpy_s(newHeader.name, dir->headers[oldFile].name);
		newHeader.size = size;
		newHeader.virtualSize = size;
		newHeader.next = oldPosition;
		// Increment the version!
		newHeader.version = dir->headers[oldFile].version + 1;
		newHeader.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

		// Write the header and set the directory info
		dir->headers[oldFile] = newHeader;
		dir->files[oldFile] = newPosition;
		writeHeader(oldFile);
	}

	return newPosition;
}

// Insert or update a files metadata and write the header to disk
File STORAGE::Filesystem::insertHeader(const char *name) {
	File newFile;
	{
		std::lock_guard<std::mutex> lk(insertGuard); // Avoid potential data races here

		// The file index is returned to the caller...
		newFile = dir->nextSpot;
		dir->nextSpot++;

		FileSize totalSize = FileHeader::SIZE;
		FilePosition position;
		FileHeader header;

		if (dir->tempList == 0) {
			position = dir->nextRawSpot;
			dir->nextRawSpot += totalSize;
			dir->tempList = position;
			header.next = 0;
		}
		else {
			FilePosition lst = dir->tempList;
			FileHeader h;
			bool found = false;
			while (lst != 0) {
				h = readHeader(lst);
				auto it = lookup.find(std::string(h.name));
				if (it != lookup.end() && dir->files[it->second] != lst) {
					found = true;
					break;
				}
				lst = h.next;
			}

			// Write the header where the old header was before being relocated
			if (found) {
				position = lst;
			}
			else {
				position = dir->nextRawSpot;
				dir->nextRawSpot += totalSize;
				header.next = dir->tempList;
				dir->tempList = position;
			}
		}

		// Copy over some metadata
		strcpy_s(header.name, name);
		header.size = 0;
		header.virtualSize = 0;
		header.version = -1;  // The file is new, but we haven't written to it yet, so it's not even version 0
		header.next = 0;
		header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

		// Setup directory
		dir->headers[newFile] = header;
		dir->files[newFile] = position;
		dir->numFiles++;

		// Add file to lookup
		lookup[std::string(name)] = newFile;

		// Write the header
		writeHeader(header, position);
	}

	return newFile;
}

// Select a file from the filesystem to use.
File &STORAGE::Filesystem::select(std::string fname) {
	std::lock_guard<std::mutex> lk(selectLock);

	bool fileExists = exists(fname);

	// Check if the file exists.
	if (!fileExists) {
		// This potentially creates garbage if the user doesn't ever write to the file
		createNewFile(fname);
	}

	return lookup[fname];
}

// Lock the file for either read or write
void STORAGE::Filesystem::lock(File file, IO::LockType type) {
	std::thread::id id = std::this_thread::get_id();

#if logging
	std::ostringstream os;
	os << "Thread " << id << " is locking " << file;
	logEvent(THREAD, os.str());
#else
	//std::cout << "Thread " << id << " is locking " << file << " for " << LockTypeToString(type) << std::endl;
#endif

	// We are locking the file so that we can read and/or write
	FileLock &fl = dir->locks[file];
	{
	std::unique_lock<std::mutex> lk(dirLock);
		// Wait until the file is available
		fl.cond.wait(lk, [&] 
		{
			if (shuttingDown) { return true; }

			// Special cases for multiversion concurrency control
			if (MVCC) {
				if (type == IO::SHARED) {
					// If the file is unlocked with no writers, we can immediately read
					bool readUnlockTest = fl.writers == 0 && dir->headers[file].version > -1;
					if (readUnlockTest) { return true; }
					// If there are writers, but there is a previous version available, we can read it
					bool readLockTest = fl.writers > 0 && dir->headers[file].version > 0;
					if (readLockTest) { return true; }
				} else if (type == IO::EXCLUSIVE) {
					// The file will get a new version, immediately stop waiting
					return true;
				}
			} else {
				// If we want read (non-exclusive) access, there must not be any writers
				bool readTest = type == IO::SHARED && fl.writers == 0;
				if (readTest) { return true; }

				// If we want write (exclusive) access, there must not be any unlocked readers or locked writers
				bool writeTest = type == IO::EXCLUSIVE && fl.readers == 0 && fl.writers == 0;
				if (writeTest) { return true; }
			}

			// Keep waiting
			return false;
		});

		if (type == IO::EXCLUSIVE) {
			fl.writers++;
		} else if (type == IO::SHARED) {
			fl.readers++;
		}
	}
}

void STORAGE::Filesystem::unlock(File file, IO::LockType type) {
	std::thread::id id = std::this_thread::get_id();

#if logging
	std::ostringstream os;
	os << "Thread " << id << " is unlocking " << file;
	logEvent(THREAD, os.str());
#else
	//std::cout << "Thread " << id << " is unlocking " << file << " for " << LockTypeToString(type) << std::endl;
#endif

	FileLock &fl = dir->locks[file];
	{
		std::unique_lock<std::mutex> lk(dirLock);
		if (type == IO::EXCLUSIVE) {
			fl.writers--;
		} else if (type == IO::SHARED) {
			fl.readers--;
		}
	}
	fl.cond.notify_one();

	std::ostringstream os2;
	os2 << "Thread " << id << " unlocked " << file;
	logEvent(THREAD, os2.str());
}

void STORAGE::Filesystem::checkFreeList() {
	FilePosition spot = dir->tempList;
	while (spot != 0) {
		const FileHeader h = readHeader(spot);
		//std::cout << h << std::endl;
		spot = h.next;
	}
}

// File existence check
bool STORAGE::Filesystem::exists(std::string name) {
	return lookup.find(name) != lookup.end();
}

double STORAGE::Filesystem::getThroughput(CountType ctype) {
	if (ctype == NUMWRITES) {
		return IO::numWrites.load() / IO::writeTime.load();
	} else if (ctype == BYTESWRITTEN) {
		return IO::bytesWritten.load() / IO::writeTime.load();
	} else if (ctype == NUMREADS) {
		return IO::numReads.load() / IO::readTime.load();
	} else if (ctype == BYTESREAD) {
		return IO::bytesRead.load() / IO::readTime.load();
	} else if (ctype == WRITETIME) {
		return IO::writeTime.load();
	} else if (ctype == READTIME) {
		return IO::readTime.load();
	} else {
		return 0.0;
	}
}

size_t STORAGE::Filesystem::count(CountType type) {
	if (type == BYTESWRITTEN) {
		return IO::bytesWritten.load();
	} else if (type == NUMWRITES) {
		return IO::numWrites.load();
	} else if (type == BYTESREAD) {
		return IO::bytesRead.load();
	} else if (type == NUMREADS) {
		return IO::numReads.load();
	} else if (type == FILES) {
		return dir->numFiles;
	} else {
		return 0;
	}
}

void STORAGE::Filesystem::shutdown(int code) {
	shuttingDown = true;
	writeFileDirectory(dir); // Make sure that any changes to the directory are flushed to disk.
	file.shutdown(code);
}

void STORAGE::Filesystem::writeFileDirectory(FileDirectory *fd) {
	logEvent(EVENT, "Writing file directory");
	char *buffer = (char*)malloc(FileDirectory::SIZE);
	if (buffer == NULL) {
		logEvent(ERROR, "Memory allocation failed.");
		return;
	}
	FilePosition pos = 0;
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->numFiles), sizeof(FileIndex));
	pos += sizeof(FileIndex);
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->nextSpot), sizeof(File));
	pos += sizeof(File);
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->nextRawSpot), sizeof(FilePosition));
	pos += sizeof(FilePosition);
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->tempList), sizeof(FilePosition));
	pos += sizeof(FilePosition);

	for (FileIndex i = 0; i < dir->numFiles; ++i) {
		memcpy(buffer + pos, reinterpret_cast<char*>(&fd->files[i]), sizeof(FilePosition));
		pos += sizeof(FilePosition);
	}

	file.raw_write(buffer, FileDirectory::SIZE, 0);

	free(buffer);
}

STORAGE::FileDirectory *STORAGE::Filesystem::readFileDirectory() {
	logEvent(EVENT, "Reading file directory");
	char *buffer = file.raw_read(0, FileDirectory::SIZE);
	FileDirectory *directory = new FileDirectory();
	FilePosition pos = 0;

	memcpy(&directory->numFiles, buffer + pos, sizeof(FileIndex));
	pos += sizeof(FileIndex);
	memcpy(&directory->nextSpot, buffer + pos, sizeof(File));
	pos += sizeof(File);
	memcpy(&directory->nextRawSpot, buffer + pos, sizeof(FilePosition));
	pos += sizeof(FilePosition);
	memcpy(&directory->tempList, buffer + pos, sizeof(FilePosition));
	pos += sizeof(FilePosition);

	for (FileIndex i = 0; i < directory->numFiles; ++i) {
		memcpy(&directory->files[i], buffer + pos, sizeof(FilePosition));
		pos += sizeof(FilePosition);
	}

	for (FileIndex i = directory->numFiles; i < MAXFILES; ++i) {
		directory->headers[i].size = 0;
		directory->headers[i].version = -1;
		directory->headers[i].next = 0;
	}

	free(buffer);
	return directory;
}

STORAGE::IO::Writer STORAGE::Filesystem::getWriter(File f) {
	return IO::Writer(this, f);
}

STORAGE::IO::Reader STORAGE::Filesystem::getReader(File f) {
	return IO::Reader(this, f);
}

STORAGE::IO::SafeWriter STORAGE::Filesystem::getSafeWriter(File f) {
	return IO::SafeWriter(this, f);
}

STORAGE::IO::SafeReader STORAGE::Filesystem::getSafeReader(File f) {
	return IO::SafeReader(this, f);
}

STORAGE::FileHeader STORAGE::Filesystem::getHeader(File f) {
	return dir->headers[f];
}
