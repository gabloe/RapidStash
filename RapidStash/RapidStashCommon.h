/*
*  RapidStashCommon.h
*  Extra definitions that don't really beong anywhere else
*
*  Written by: Gabriel J. Loewen
*/

#ifndef _RAPIDSTASHCOMMON_H_
#define _RAPIDSTASHCOMMON_H_
#pragma once

#include <sys/stat.h>

#define SUCCESS 0
#define FAILURE 1

// Attempt at supporting cross compatibility with POSIX systems
#if defined(linux) || defined(__linux) || defined(apple) || defined(__apple)
#include <sys/types.h>
#include <fcntl.h>
#define strcpy_s strcpy
#define _close close
#define _tell(fd) lseek(fd,0,SEEK_CUR)
#define _lseek lseek
#define _O_RDWR O_RDWR
#define _O_BINARY 1
#define _O_RANDOM 1
#define _O_CREAT O_CREAT
#define _SH_DENYNO 1
#define _S_IREAD S_IREAD
#define _S_IWRITE S_IWRITE
static int _sopen_s(int *fd, const char *fname, int oflags, int shflags, int pmode)
{                                                 
	*fd = open(fname,oflags);                 
	return 0;                                
}
#else

#include <sstream>

template<typename T> 
std::string toString(T s) {
	std::stringstream ss;
	ss << s;
	return ss.str();
}
#endif

// Typedefs to make testing easier
typedef int File;
typedef size_t FileSize;
typedef size_t FilePosition;
typedef int FileVersion;
typedef int FileIndex;
#endif
