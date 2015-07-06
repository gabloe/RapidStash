#include "Filesystem.h"

STORAGE::Filesystem::Filesystem(const char* fname) : file(fname) {
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
			logEvent(EVENT, name);
			lookup[name] = i;
		}
	}
}


STORAGE::File STORAGE::Filesystem::createNewFile(std::string fname) {
	logEvent(EVENT, "Creating file: " + fname);
	size_t nameLen = fname.size();
	if (nameLen > FileMeta::MAXNAMELEN) {
		nameLen = FileMeta::MAXNAMELEN;
	}

	// Construct the file object.
	unsigned short index = dir->insert(fname);

	// Get the position of the new file
	off_t pos = dir->files[index].position;

	// Raw write a placeholder.  The actual file contents will be written later.
	file.raw_write(FilePlaceholder, sizeof(FilePlaceholder), pos);

	// Update lookup table
	lookup[fname] = index;

#ifdef LOGDEBUGGING
	logEvent(EVENT, "Verifying file placeholder");
	// Some testing to make sure we are writing the correct stuff
	char *test = file.raw_read(pos, sizeof(FilePlaceholder));
	for (int i = 0; i < sizeof(FilePlaceholder); ++i) {
		if (test[i] != FilePlaceholder[i]) {
			logEvent(ERROR, "Error verifying file placeholder");
			shutdown(FAILURE);
		}
	}
	logEvent(EVENT, "File placeholder verified");

#endif

	return File(index);
}

STORAGE::File STORAGE::Filesystem::select(std::string fname) {
	dirLock.lock();
	std::map<std::string, unsigned short>::iterator it;
	// Check if the file exists.
	if ((it = lookup.find(fname)) != lookup.end()) {
		logEvent(EVENT, "File " + fname + " exists");
		dirLock.unlock();
		return File(lookup[fname]);
	} else {
		// File doesn't exist.  Create it.
		STORAGE::File file = createNewFile(fname);
		dirLock.unlock();
		return file;
	}
}

void STORAGE::Filesystem::lock(File file) {
	using namespace std::literals;

#ifdef LOGDEBUGGING
	static int count;
#endif

	// We are locking the file so that we can read and/or write
	dirLock.lock();  // START CRITICAL REGION
	FileMeta &meta = dir->files[file];

	meta.numLocks++;
	while (meta.lock == true) {	/* If the file is locked, we have to wait to read or write */
		dirLock.unlock();
		std::this_thread::sleep_for(100ms);  // Simple busy wait, should probably make this suspend the thread instead
		// If this continues for a prolonged period of time there could be potential deadlock.
#ifdef LOGDEBUGGING
		count++;
		if (count > DEADLOCKTHRESHHOLD) {
			logEvent(WARNING, "Potential deadlock detected");
		}
#endif
		dirLock.lock();
	}

	meta.lock = true;
	dirLock.unlock();
	// END CRITICAL REGION
}

void STORAGE::Filesystem::unlock(File file) {
	// We are locking the file so that we can read and/or write
	dirLock.lock();

	FileMeta &meta = dir->files[file];
	if (meta.lock) {	/* If the file is locked, decrement the number of locking threads */
		meta.numLocks--;
	}

	// Signal a thread that is busy waiting to get the lock.
	meta.lock = false;

	//dirLock.unlock();
	dirLock.unlock();
}

void STORAGE::Filesystem::shutdown(int code) {
	dirLock.lock();
	writeFileDirectory(dir); // Make sure that any changes are flushed to disk.
	dirLock.unlock();
	file.shutdown(code);
}

void STORAGE::Filesystem::writeFileDirectory(FileDirectory *fd) {
	logEvent(EVENT, "Writing file directory");
	file.raw_write(reinterpret_cast<char*>(fd), FileDirectory::SIZE, 0);
}

STORAGE::FileDirectory *STORAGE::Filesystem::readFileDirectory() {
	logEvent(EVENT, "Reading file directory");
	char *buffer = file.raw_read(0, FileDirectory::SIZE);
	FileDirectory *directory = new FileDirectory();
	memcpy(directory, reinterpret_cast<FileDirectory*>(buffer), FileDirectory::SIZE);
	free(buffer);
	return directory;
}