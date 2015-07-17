#ifndef _FILEIOCOMMON_H_
#define _FILEIOCOMMON_H_
#pragma once
#include <atomic>
#include <exception>

namespace STORAGE {
	namespace IO {

		// Statics used by the filesystem
		static bool timingEnabled = true;
		static std::atomic<size_t> bytesWritten;								// Count of number of bytes written
		static std::atomic<size_t> numWrites;									// Count of number of write operations
		static std::atomic<size_t> bytesRead;									// Count of number of bytes read
		static std::atomic<size_t> numReads;									// Count of number of read operations
		static std::atomic<double> writeTime;									// Sum of write durations
		static std::atomic<double> readTime;									// Sum of read durations

		enum StartLocation {
			BEGIN,
			END
		};

		/*
		*  Exceptions!
		*/
		class SeekOutOfBoundsException : public std::exception {
			virtual const char* what() const throw() {
				return "Attempted to seek beyond the end of the file or before the beginning of the file.";
			}
		};

		class ReadOutOfBoundsException : public std::exception {
			virtual const char* what() const throw() {
				return "Attempted to read beyond the end of the file or before the beginning of the file.";
			}
		};
	}
}
#endif
