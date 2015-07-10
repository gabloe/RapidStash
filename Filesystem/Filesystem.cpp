/*
*  Filesystem.cpp
*  Enables a basic Filesystem to be managed on top of a managed memory mapped file
*
*  Written by: Gabriel Loewen
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
			lookup[name] = i;
		}
	}
}

// Read the header for a particular file from disk
STORAGE::FileHeader STORAGE::Filesystem::readHeader(File f) {
	FilePosition &pos = dir->files[f].position;
	STORAGE::FileHeader header;
	char *buffer = file.raw_read(pos, FileHeader::SIZE);

	size_t offset = 0;
	memcpy(header.name, buffer, FileHeader::MAXNAMELEN);
	offset += FileHeader::MAXNAMELEN;
	memcpy(&header.size, buffer + offset, sizeof(FileSize));
	offset += sizeof(FileSize);
	memcpy(&header.virtualSize, buffer + offset, sizeof(FileSize));
	free(buffer);

	return header;
}

// Write a files header to disk
void STORAGE::Filesystem::writeHeader(File f) {
	FilePosition &pos = dir->files[f].position;
	FileHeader &header = dir->headers[f];

	char *buffer = (char*)malloc(FileHeader::SIZE);
	if (buffer == NULL) {
		logEvent(ERROR, "Memory allocation failed.");
		return;
	}
	size_t offset = 0;
	memcpy(buffer + offset, header.name, FileHeader::MAXNAMELEN);
	offset += FileHeader::MAXNAMELEN;
	memcpy(buffer + offset, reinterpret_cast<char*>(&header.size), sizeof(FileSize));
	offset += sizeof(FileSize);
	memcpy(buffer + offset, reinterpret_cast<char*>(&header.virtualSize), sizeof(FileSize));
	file.raw_write(buffer, FileHeader::SIZE, pos);
	free(buffer);
}

// Create a new file
File STORAGE::Filesystem::createNewFile(std::string fname) {
	logEvent(EVENT, "Creating file: " + fname);
	if (fname.size() > FileHeader::MAXNAMELEN) {
		fname = std::string(fname, FileHeader::MAXNAMELEN);
	}

	return insert(fname.c_str());
}

// Insert or update a files metadata and write the header to disk
File STORAGE::Filesystem::insert(const char *name, FileSize size, File oldFile, bool reuse) {
	std::lock_guard<std::mutex> lk(insertGuard);

	File spot = oldFile;
	FileSize totalSize = FileHeader::SIZE + size;

	// If the file is new
	if (!reuse) {
		spot = dir->firstFree++;
		dir->numFiles++;
		dir->files[spot].lock = false;
		dir->headers[spot].size = 0;
	} else {
		dir->headers[spot].size = size;
	}

	dir->files[spot].position = dir->nextRawSpot;
	dir->nextRawSpot += totalSize;

	memcpy(dir->headers[spot].name, name, FileHeader::MAXNAMELEN);
	dir->headers[spot].virtualSize = size;
	writeHeader(spot);

	lookup[std::string(name)] = spot;

	return spot;
}

// Select a file from the filesystem to use
File STORAGE::Filesystem::select(std::string fname) {
	std::lock_guard<std::mutex> lk(dirLock);

	size_t fileExists = lookup.count(fname);
	File file;

	// Check if the file exists.
	if (fileExists) {
		logEvent(EVENT, "File " + fname + " exists");
		file = lookup[fname];
	} else {
		// File doesn't exist.  Create it.
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
	FileMeta &meta = dir->files[file];
	std::unique_lock<std::mutex> lk(dirLock);
	{
		// Wait until the file is available (not locked)
		meta.cond.wait(lk, [&meta, &type] 
			{
				// If we want write (exclusive) access, there must not be any unlocked readers
				bool writeRequest = !meta.lock && type == WRITE && meta.readers == 0;

				// If we want read (non-exclusive) access, there must not be any writers
				bool readRequest = !meta.lock && type == READ && meta.writers == 0;

				return writeRequest || readRequest;
			});
		if (type == WRITE) {
			meta.lock = true;
			meta.tid = id;
			meta.writers++;
		} else {
			meta.readers++;
		}
	}
	lk.unlock();
}

void STORAGE::Filesystem::unlock(File file, LockType type) {
	std::thread::id id = std::this_thread::get_id();
	std::ostringstream os;
	os << "Thread " << id << " is unlocking " << file;
	logEvent(THREAD, os.str());

	FileMeta &meta = dir->files[file];
	std::unique_lock<std::mutex> lk(dirLock);
	{
		// Wait until the file is actually locked and the current thread is the owner of the lock
		meta.cond.wait(lk, [&meta, &id, &type] 
			{
				bool writeRequest = type == WRITE && meta.lock && meta.tid == id;
				bool readRequest = type == READ;
				return writeRequest || readRequest; 
			});
		if (type == WRITE) {
			meta.lock = false;
			meta.tid = nobody;
			meta.writers--;
		} else if (type == READ) {
			meta.readers--;
		}
	}
	lk.unlock();
	meta.cond.notify_one();

	std::ostringstream os2;
	os2 << "Thread " << id << " unlocked " << file;
	logEvent(THREAD, os2.str());
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
		logEvent(ERROR, "Memory allocation failed.");
		return;
	}
	FilePosition pos = 0;
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->numFiles), sizeof(File));
	pos += sizeof(File);
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->firstFree), sizeof(File));
	pos += sizeof(File);
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->nextRawSpot), sizeof(FilePosition));
	pos += sizeof(FilePosition);

	for (FileIndex i = 0; i < dir->numFiles; ++i) {
		memcpy(buffer + pos, reinterpret_cast<char*>(&fd->files[i].position), sizeof(FilePosition));
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

	memcpy(&directory->numFiles, buffer + pos, sizeof(File));
	pos += sizeof(File);
	memcpy(&directory->firstFree, buffer + pos, sizeof(File));
	pos += sizeof(File);
	memcpy(&directory->nextRawSpot, buffer + pos, sizeof(FilePosition));
	pos += sizeof(FilePosition);

	for (FileIndex i = 0; i < directory->numFiles; ++i) {
		memcpy(&directory->files[i].position, buffer + pos, sizeof(FilePosition));
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
	FilePosition &loc = fs->dir->files[file].position;
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
	FilePosition loc = fs->dir->files[file].position;
	FileSize &virtualSize = fs->dir->headers[file].virtualSize;
	FileSize oldSize = fs->dir->headers[file].size;

	if (!writeTimeStarted.load()) {
		startWriteTime = std::chrono::high_resolution_clock::now();
		writeTimeStarted.store(true);
	}

	bytesWritten += size;
	numWrites++;

	// If there is not enough excess space available, we must create a new file for this write
	// This generates garbage that may eventually need to be cleaned up.
	if (size > virtualSize) {
		fs->insert(header.name, size, file, true);
		FilePosition newLoc = fs->dir->files[file].position;
		// If we are writing somewhere in the middle of the file, we have to copy over some of the beginning
		// of the old file.
		if (position > 0) {
			char *chunk = fs->file.raw_read(loc + FileHeader::SIZE, position);
			fs->file.raw_write(chunk, position, newLoc + FileHeader::SIZE);
		}

		// Write the rest of the data
		fs->file.raw_write(data, size, newLoc + position + FileHeader::SIZE);
	} else {
		// Update metadata in directory
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
	FilePosition &loc = fs->dir->files[file].position;
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

	if (!readTimeStarted.load()) {
		startReadTime = std::chrono::high_resolution_clock::now();
		readTimeStarted.store(true);
	}

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
	FilePosition &loc = fs->dir->files[file].position;
	FileSize &size = fs->dir->headers[file].size;

	// We don't want to be able to read beyond the last byte of the file.
	if (position + amt > size) {
		throw ReadOutOfBoundsException();
	}

	bytesRead += amt;
	numReads++;

	char *data = fs->file.raw_read(loc + position + FileHeader::SIZE, amt);

	return data;
}