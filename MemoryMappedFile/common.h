#ifndef _COMMON_H_
#define _COMMON_H_
#pragma once

#include <sys/stat.h>

#define SUCCESS 0
#define FAILURE 1

// Typedefs to make testing easier
typedef int File;
typedef size_t FileSize;
typedef size_t FilePosition;
typedef int FileIndex;

bool fileExists(const char*);
#endif
