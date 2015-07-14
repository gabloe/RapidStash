/*
*  Filesystem.cpp
*  Enables a basic Filesystem to be managed on top of a managed memory mapped file
*
*  Written by: Gabriel J. Loewen
*/

#include "Filesystem.h"

// Constructor
STORAGE::Filesystem::Filesystem(const char* fname) : file(fname) {
	bytesWritten.store(0);
	bytesRead.store(0);
	numWrites.store(0);
	numReads.store(0);
	writeTimeStarted.store(false);
	readTimeStarted.store(false);

	// Set up file directory if the backing file is new.
	if (file.isNew()) {
		logEvent(EVENT, "Backing file is new");
		dir = new FileDirectory();
		writeFileDirectory(dir);
	} else {
		logEvent(EVENT, "Backing file exists, populating lookup table");
		dir = readFileDirectory();
		std::ostringstream os;
		os << dir->numFiles;
		logEvent(EVENT, "Number of stored files is " + os.str());

		// Populate lookup table
		for (File i = 0; i < dir->numFiles; ++i) {
			dir->headers[i] = readHeader(i);
			std::string name(dir->headers[i].name);
//std::cout << "Found " << name << " at " << dir->files[i] << std::endl;
			lookup[name] = i;
		}
	}
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
	memcpy(&header.version, buffer + offset, sizeof(FileVersion));
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
	memcpy(buffer + offset, reinterpret_cast<char*>(&header.version), sizeof(FileVersion));
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
	std::lock_guard<std::mutex> lk(insertGuard);

	FileSize totalSize = size + FileHeader::SIZE;
	FilePosition oldPosition = dir->files[oldFile];

	FilePosition newPosition = dir->nextRawSpot;
	dir->nextRawSpot += totalSize;

	FileHeader newHeader;

	strcpy_s(newHeader.name, dir->headers[oldFile].name);
	newHeader.size = size;
	newHeader.version++;
	newHeader.next = oldPosition;
	dir->files[oldFile] = newPosition;
	dir->headers[oldFile] = newHeader;
	writeHeader(newHeader, newPosition);

	return newPosition;
}

// Insert or update a files metadata and write the header to disk
File STORAGE::Filesystem::insertHeader(const char *name) {
	std::lock_guard<std::mutex> lk(insertGuard); // Avoid potential data races here
	FileSize totalSize = FileHeader::SIZE;
	
	FilePosition position;

	// The file index is returned to the caller...
	File newFile = dir->nextSpot;
	dir->nextSpot++;

	if (dir->tempList == 0) {
		position = dir->nextRawSpot;
		dir->nextRawSpot += totalSize;
		dir->tempList = position;
		dir->headers[newFile].next = 0;
	} else {
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
		if (found) {
			position = lst;
		} else {
			position = dir->nextRawSpot;
			dir->nextRawSpot += totalSize;
			dir->headers[newFile].next = dir->tempList;
			dir->tempList = position;
		}
	}

	// Copy over some metadata
	strcpy_s(dir->headers[newFile].name, name);
	dir->headers[newFile].size = 0;
	dir->headers[newFile].version = -1;  // The file is new, but we haven't written to it yet, so it's not even version 0

	// Setup directory
	dir->locks[newFile].lock = false;
	dir->files[newFile] = position;
	dir->numFiles++;

	// Write the header
	writeHeader(newFile);
	lookup[std::string(name)] = newFile;

	return newFile;
}

// Select a file from the filesystem to use.  Optionally, pass in the amount of space to be allocated
// if the file needs to be created, otherwise the newly created file will be allocated with the default
// size.  This is useful if you know the size of the data you will write beforehand.
File STORAGE::Filesystem::select(std::string fname) {
	std::lock_guard<std::mutex> lk(dirLock);

	bool fileExists = exists(fname);
	File file;

	// Check if the file exists.
	if (fileExists) {
		logEvent(EVENT, "File " + fname + " exists");
		file = lookup[fname];
	} else {
		// This potentially creates garbage if the user doesn't ever write to the file
		file = createNewFile(fname);
	}

	return file;
}

// Lock the file for either read or write
void STORAGE::Filesystem::lock(File file, LockType type) {
	std::thread::id id = std::this_thread::get_id();
	std::ostringstream os;
	os << "Thread " << id << " is locking " << file;
	logEvent(THREAD, os.str());

	// We are locking the file so that we can read and/or write
	FileLock &fl = dir->locks[file];
	std::unique_lock<std::mutex> lk(dirLock);
	{
		// Wait until the file is available (not locked)
			fl.cond.wait(lk, [&fl, &type] 
			{
				// If we want write (exclusive) access, there must not be any unlocked readers
				bool writeRequest = !fl.lock && type == WRITE && fl.readers == 0;

				// If we want read (non-exclusive) access, there must not be any writers
				bool readRequest = !fl.lock && type == READ && fl.writers == 0;

				return writeRequest || readRequest;
			});
		if (type == WRITE) {
			fl.lock = true;
			fl.tid = id;
			fl.writers++;
		} else {
			fl.readers++;
		}
	}
	lk.unlock();
}

void STORAGE::Filesystem::unlock(File file, LockType type) {
	std::thread::id id = std::this_thread::get_id();
	std::ostringstream os;
	os << "Thread " << id << " is unlocking " << file;
	logEvent(THREAD, os.str());

	FileLock &fl = dir->locks[file];
	std::unique_lock<std::mutex> lk(dirLock);
	{
		// Wait until the file is actually locked and the current thread is the owner of the lock
		fl.cond.wait(lk, [&fl, &id, &type] 
			{
				bool writeRequest = type == WRITE && fl.lock && fl.tid == id;
				bool readRequest = type == READ;
				return writeRequest || readRequest; 
			});
		if (type == WRITE) {
			fl.lock = false;
			fl.tid = nobody;
			fl.writers--;
		} else if (type == READ) {
			fl.readers--;
		}
	}
	lk.unlock();
	fl.cond.notify_one();

	std::ostringstream os2;
	os2 << "Thread " << id << " unlocked " << file;
	logEvent(THREAD, os2.str());
}

// File existence check
bool STORAGE::Filesystem::exists(std::string name) {
	return lookup.find(name) != lookup.end();
}

size_t STORAGE::Filesystem::count(CountType type) {
	if (type == BYTESWRITTEN) {
		return bytesWritten.load();
	} else if (type == WRITES) {
		return numWrites.load();
	} else if (type == BYTESREAD) {
		return bytesRead.load();
	} else if (type == READS) {
		return numReads.load();
	} else if (type == FILES) {
		return dir->numFiles;
	} else {
		return 0;
	}
}

double STORAGE::Filesystem::getWriteTurnaround() {
	std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	auto turnaround = std::chrono::duration_cast<std::chrono::duration<double>>(end - startWriteTime);
	return turnaround.count();
}

double STORAGE::Filesystem::getReadTurnaround() {
	std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	auto turnaround = std::chrono::duration_cast<std::chrono::duration<double>>(end - startReadTime);
	return turnaround.count();
}

void STORAGE::Filesystem::shutdown(int code) {
	writeFileDirectory(dir); // Make sure that any changes to the directory are flushed to disk.
	file.shutdown(code);
}

void STORAGE::Filesystem::writeFileDirectory(FileDirectory *fd) {
	logEvent(EVENT, "Writing file directory");
	char *buffer = (char*)malloc(FileDirectory::SIZE);
	if (buffer == NULL) {
		logEvent(ERROR,r "Memory allocation failed.");
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

	free(buffer);
	return directory;
}

STORAGE::Writer STORAGE::Filesystem::getWriter(File f) {
	return STORAGE::Writer(this, f);
}

STORAGE::Reader STORAGE::Filesystem::getReader(File f) {
	return STORAGE::Reader(this, f);
}

STORAGE::FileHeader STORAGE::Filesystem::getHeader(File f) {
	return dir->headers[f];
}

/*
 *  File writer utility class
 */
FilePosition STORAGE::Writer::tell() {
	return position;
}

void STORAGE::Writer::seek(off_t pos, StartLocation start) {
	FilePosition &loc = fs->dir->files[file];
	FileSize &len = fs->dir->headers[file].size;

	if (start == BEGIN) {
		if (pos > len || pos < 0) {
			throw SeekOutOfBoundsException();
		}
		position = loc + pos;
	} else if (start == END) {
		if (len + pos > len || len + pos < 0) {
			throw SeekOutOfBoundsException();
		}
		position = loc + len + pos;
	}
}

void STORAGE::Writer::write(const char *data, FileSize size) {
	FilePosition loc = fs->dir->files[file];
	FileSize oldSize = fs->dir->headers[file].size;

	if (!writeTimeStarted.load()) {
		startWriteTime = std::chrono::high_resolution_clock::now();
		writeTimeStarted.store(true);
	}

	bytesWritten += size;
	numWrites++;

	// If there is not enough excess space available, we must create a new file for this write
	// This generates garbage that may eventually need to be cleaned up.
	// OR if MVCC is enabled
	if (size > oldSize || MVCC) {
		FilePosition newLoc = fs->relocateHeader(file, size);

		// If we are writing somewhere in the middle of the file, we have to copy over some of the beginning
		// of the old file.
		if (position > 0) {
			char *chunk = fs->file.raw_read(loc + FileHeader::SIZE, position);
			fs->file.raw_write(chunk, position, newLoc + FileHeader::SIZE);
		}

		// Write the rest of the data
		fs->file.raw_write(data, size, newLoc + position + FileHeader::SIZE);
	} else {
		// If we aren't using MVCC and the old file size is accommodating just update metadata in directory
		fs->dir->headers[file].size = size;
		fs->writeHeader(file);

		// Write the data
		fs->file.raw_write(data, size, loc + position + FileHeader::SIZE);
	}
}

/*
*  File reader utility class
*/
FilePosition STORAGE::Reader::tell() {
	return position;
}

void STORAGE::Reader::seek(off_t pos, StartLocation start) {
	FilePosition &loc = fs->dir->files[file];
	FileSize &len = fs->dir->headers[file].size;

	if (start == BEGIN) {
		if (pos > len || pos < 0) {
			throw SeekOutOfBoundsException();
		}
		position = loc + pos;
	}
	else if (start == END) {
		if (len + pos > len || len + pos < 0) {
			throw SeekOutOfBoundsException();
		}
		position = loc + len + pos;
	}
}

char *STORAGE::Reader::read() {
	FileSize &size = fs->dir->headers[file].size;

	char *buffer = NULL;
	try {
		buffer = read(size);
	} catch (ReadOutOfBoundsException) {
		logEvent(ERROR, "Read out of bounds");
		// Generate bogus buffer
		buffer = (char*)malloc(size);
		if (buffer == NULL) {
			logEvent(ERROR, "Memory allocation failed.");
			return NULL;
		}
		memset(buffer, 0, size);
	}
	return buffer;
}

char *STORAGE::Reader::read(FileSize amt) {
	FilePosition &loc = fs->dir->files[file];
	FileSize &size = fs->dir->headers[file].size;

	// We don't want to be able to read beyond the last byte of the file.
	if (position + amt > size) {
		throw ReadOutOfBoundsException();
	}

	if (!readTimeStarted.load()) {
		startReadTime = std::chrono::high_resolution_clock::now();
		readTimeStarted.store(true);
	}

	bytesRead += amt;
	numReads++;

	char *data = fs->file.raw_read(loc + position + FileHeader::SIZE, amt);

	return data;
}