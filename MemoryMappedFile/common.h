#ifndef _COMMON_H_
#define _COMMON_H_
#pragma once

#include <sys/stat.h>

#define SUCCESS 0
#define FAILURE 1

typedef unsigned short File;
typedef size_t FileSize;
typedef size_t FilePosition;
typedef unsigned short FileIndex;
typedef char byte;

bool fileExists(const char*);
#endif
