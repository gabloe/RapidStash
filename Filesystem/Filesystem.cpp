#include "Filesystem.h"

STORAGE::Filesystem::Filesystem(const char* fname) : file(fname) {
	// Set up file directory if the backing file is new.
	if (file.isNew()) {
		dir = new FileDirectory();
		writeFileDirectory(dir);
	} else {
		dir = readFileDirectory();

		// Populate lookup table
		for (int i = 0; i < dir->numFiles; ++i) {
			std::string name(dir->files[i].name, dir->files[i].nameSize);
			lookup[name] = i;
		}
	}
}


STORAGE::File STORAGE::Filesystem::createNewFile(std::string fname) {
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
	std::map<std::string, unsigned short>::iterator it;
	// Check if the file exists.
	if ((it = lookup.find(fname)) != lookup.end()) {
		return File(lookup[fname]);
	} else {
		// File doesn't exist.  Create it.
		return createNewFile(fname);
	}
}

void STORAGE::Filesystem::lock(File file, Mode mode) {
	using namespace std::literals;
	// START CRITICAL REGION

	// We are locking the file so that we can read and/or write
	FileMeta &meta = dir->files[file.index];
	meta.numLocks++;
	while (meta.writeLock) {	/* If the file is locked, we have to wait to read or write */
		std::this_thread::sleep_for(1s);
	}

	if (mode == WRITE) {
		meta.writeLock = true;
	}

	// END CRITICAL REGION
}

void STORAGE::Filesystem::unlock(File file, Mode mode) {
	// START CRITICAL REGION

	// We are locking the file so that we can read and/or write
	FileMeta &meta = dir->files[file.index];
	if (meta.writeLock) {	/* If the file is locked, decrement the number of locking threads */
		meta.numLocks--;
	}

	if (meta.numLocks == 0) {
		meta.writeLock = false;
	}

	// END CRITICAL REGION
}

void STORAGE::Filesystem::shutdown(int code) {
	writeFileDirectory(dir);
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