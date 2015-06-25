#ifndef _COMMON_H_
#define _COMMON_H_
#pragma once

#include <sys/stat.h>

#define SUCCESS 0
#define FAILURE 1

bool fileExists(const char* fname) {
	struct stat buffer;
	return (stat(fname, &buffer) == 0);
}

#endif
