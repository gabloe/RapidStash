#include "Filesystem.h"

STORAGE::Filesystem::Filesystem(const char* fname) : file(fname) {
	bytesWritten.store(0);
	numWrites.store(0);
	timeStarted.store(false);

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
			std::string name(dir->files[i].name, dir->files[i].nameSize);
			lookup[name] = i;
		}
	}
}


File STORAGE::Filesystem::createNewFile(std::string fname) {
	logEvent(EVENT, "Creating file: " + fname);
	if (fname.size() > FileMeta::MAXNAMELEN) {
		fname = std::string(fname, FileMeta::MAXNAMELEN);
	}

	// Construct the file object.
	File index = dir->insert(fname);

	// Get the position of the new file
	FilePosition &pos = dir->files[index].position;

	// Raw write a placeholder.  The actual file contents will be written later.
	file.raw_write(FilePlaceholder, sizeof(FilePlaceholder), pos);

	// Update lookup table
	lookup[fname] = index;

	return index;
}

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
		meta.cond.wait(lk, [&meta] {return !meta.lock; });
		meta.lock = true;
		meta.tid = id;
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
		meta.cond.wait(lk, [&meta, &id] {return meta.lock && meta.tid == id; });
		meta.lock = false;
		meta.tid = nobody;
	}
	lk.unlock();
	meta.cond.notify_one();

	std::ostringstream os2;
	os2 << "Thread " << id << " unlocked " << file;
	logEvent(THREAD, os2.str());
}

size_t STORAGE::Filesystem::count(CountType type) {
	if (type == BYTES) {
		return bytesWritten.load();
	} else if (type == WRITES) {
		return numWrites.load();
	} else {
		return 0;
	}
}

double STORAGE::Filesystem::getTurnaround() {
	std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	auto turnaround = std::chrono::duration_cast<std::chrono::duration<double>>(end - startTime);
	return turnaround.count();
}

void STORAGE::Filesystem::shutdown(int code) {
	writeFileDirectory(dir); // Make sure that any changes to the directory are flushed to disk.
	file.shutdown(code);
}

void STORAGE::Filesystem::writeFileDirectory(FileDirectory *fd) {
	logEvent(EVENT, "Writing file directory");
	char *buffer = (char*)malloc(FileDirectory::SIZE);
	FilePosition pos = 0;
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->numFiles), sizeof(File));
	pos += sizeof(File);
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->firstFree), sizeof(File));
	pos += sizeof(File);
	memcpy(buffer + pos, reinterpret_cast<char*>(&fd->nextRawSpot), sizeof(FilePosition));
	pos += sizeof(FilePosition);

	for (int i = 0; i < dir->numFiles; ++i) {
		memcpy(buffer + pos, reinterpret_cast<char*>(&fd->files[i].nameSize), sizeof(size_t));
		pos += sizeof(size_t);
		memcpy(buffer + pos, fd->files[i].name, FileMeta::MAXNAMELEN);
		pos += FileMeta::MAXNAMELEN;
		memcpy(buffer + pos, reinterpret_cast<char*>(&fd->files[i].size), sizeof(FileSize));
		pos += sizeof(FileSize);
		memcpy(buffer + pos, reinterpret_cast<char*>(&fd->files[i].virtualSize), sizeof(FileSize));
		pos += sizeof(FileSize);
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
		memcpy(&directory->files[i].nameSize, buffer + pos, sizeof(size_t));
		pos += sizeof(size_t);
		memcpy(directory->files[i].name, buffer + pos, FileMeta::MAXNAMELEN);
		pos += FileMeta::MAXNAMELEN;
		memcpy(&directory->files[i].size, buffer + pos, sizeof(FileSize));
		pos += sizeof(FileSize);
		memcpy(&directory->files[i].virtualSize, buffer + pos, sizeof(FileSize));
		pos += sizeof(FileSize);
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

FileSize STORAGE::Filesystem::getSize(File f) {
	return dir->files[f].size;
}

/*
 *  File writer utility class
 */
FilePosition STORAGE::Writer::tell() {
	return position;
}

void STORAGE::Writer::seek(off_t pos, StartLocation start) {
	FilePosition &loc = fs->dir->files[file].position;
	FileSize &len = fs->dir->files[file].size;

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
	FilePosition &loc = fs->dir->files[file].position;
	FileSize &virtualSize = fs->dir->files[file].virtualSize;

	if (!timeStarted.load()) {
		startTime = std::chrono::high_resolution_clock::now();
		timeStarted.store(true);
	}

	bytesWritten += size;
	numWrites++;

	// If there is not enough excess space available, we must create a new file for this write
	// and release the current allocated space for new files.
	if (size >= virtualSize - 1) {
		// TODO: this...
	}

	// Update metadata in directory
	fs->dir->files[file].size = size;

	// Write the data
	fs->file.raw_write(data, size, loc + position);
}

/*
*  File reader utility class
*/
FilePosition STORAGE::Reader::tell() {
	return position;
}

void STORAGE::Reader::seek(off_t pos, StartLocation start) {
	FilePosition &loc = fs->dir->files[file].position;
	FileSize &len = fs->dir->files[file].size;

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
	FileSize &size = fs->dir->files[file].size;
	char *buffer = NULL;
	try {
		buffer = read(size);
	} catch (ReadOutOfBoundsException) {
		logEvent(ERROR, "Read out of bounds");
		// Generate bogus buffer
		buffer = (char*)malloc(size);
		memset(buffer, 0, size);
	}
	return buffer;
}

char *STORAGE::Reader::read(FileSize amt) {
	FilePosition &loc = fs->dir->files[file].position;
	FileSize &size = fs->dir->files[file].size;

	// We don't want to be able to read beyond the last byte of the file.
	if (position + amt > size) {
		throw ReadOutOfBoundsException();
	}

	char *data = fs->file.raw_read(loc + position, amt);

	return data;
}