#include "Filesystem.h"

STORAGE::Filesystem::Filesystem(const char* fname) : file(fname) {
	// Set up file directory if the backing file is new.
	if (file.isNew()) {
		dir = new FileDirectory(0);
		writeFileDirectory(dir);
	} else {
		dir = readFileDirectory();
	}
}

void STORAGE::Filesystem::shutdown(int code) {
	file.shutdown(code);
}

void STORAGE::Filesystem::writeFileDirectory(FileDirectory *fd) {
	file.raw_write(reinterpret_cast<char*>(fd), FileDirectory::SIZE, 0);
}

STORAGE::FileDirectory *STORAGE::Filesystem::readFileDirectory() {
	char *buffer = file.raw_read(0, FileDirectory::SIZE);
	FileDirectory *directory = new FileDirectory(0);
	memcpy(directory, reinterpret_cast<FileDirectory*>(buffer), FileDirectory::SIZE);
	for (int i = 0; i < MAXFILES; ++i) {
		if (directory->positions[i] != -1) {
			std::ostringstream d;
			d << directory->positions[i];
			logEvent(ERROR, "Directory read failed, read " + d.str() + ", expected -1.");
			shutdown(FAILURE);
		}
	}
	free(buffer);
	return directory;
}