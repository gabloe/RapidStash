#include "common.h"

bool fileExists(const char* fname) {
	struct stat buffer;
	return (stat(fname, &buffer) == 0);
}