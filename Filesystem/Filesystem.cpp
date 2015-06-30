#include "Filesystem.h"

STORAGE::Filesystem::Filesystem(const char* fname) : file(fname) {
	// Set up file directory if the backing file is new.
	if (file.isNew()) {
		dir = new FileDirectory();
		writeFileDirectory(dir);
	} else {
		dir = readFileDirectory();
	}
}

STORAGE::File *STORAGE::Filesystem::createNewFile(std::string fname) {
	size_t nameLen = fname.size();
	if (nameLen > File::MAXNAMELEN) {
		nameLen = File::MAXNAMELEN;
	}

	// Construct the file object.
	File *f = new File();
	memcpy(f->name, fname.c_str(), nameLen);
	f->name[nameLen] = '\0';

	// Modify the file directory
	f->location = dir->insert(fname);

	// The size allocated to new files is constant
	f->virtualSize = STORAGE::FileDirectory::MINALLOCATION;

	// Raw write a placeholder byte.  The actual file contents will be written later.
	file.raw_write(FilePlaceholder, sizeof(FilePlaceholder), f->location);

#ifdef LOGDEBUGGING
	logEvent(EVENT, "Verifying file placeholder");
	// Some testing to make sure we are writing the correct stuff
	char *test = file.raw_read(f->location, sizeof(FilePlaceholder));
	for (int i = 0; i < sizeof(FilePlaceholder); ++i) {
		if (test[i] != FilePlaceholder[i]) {
			logEvent(ERROR, "Error verifying file placeholder");
			shutdown(FAILURE);
		}
	}
	logEvent(EVENT, "File placeholder verified");

#endif

	return f;
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