#ifndef _TESTING_H_
#define _TESTING_H_

#pragma once
#include <functional>
#include <iostream>
#include <string>
#include <Filesystem.h>

static const int numNames = 16;
static const int stringSize = 2048;
static const int numThreads = 32;
static const int numWriters = 256;
static const int numReaders = 256;

int TestReadWrite(STORAGE::Filesystem *);
int TestConcurrentWrite(STORAGE::Filesystem *);
int TestHeader(STORAGE::Filesystem *);
int TestMVCC(STORAGE::Filesystem *);
int TestConcurrentReadWrite(STORAGE::Filesystem *);
int TestConcurrentMultiFile(STORAGE::Filesystem *);
int TestConcurrentMultiFileMVCC(STORAGE::Filesystem *);
int TestUnlink(STORAGE::Filesystem *);

typedef std::function<void()> TestWrapper_t;

static void TestWrapper(std::string name, std::function<int(STORAGE::Filesystem*)> fn) {
	std::cout << name << ": ";
	std::string fname("data/" + name);
	STORAGE::Filesystem *fs = new STORAGE::Filesystem(fname.c_str());
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
	int res = fn(fs);
	std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
	if (res == 0) {
		std::cout << "PASSED!";
	} else {
		std::cout << "FAILED!";
	}
	std::cout << " (took " << time_span.count() << " seconds.)\n";
	fs->shutdown();
	delete fs;
}

static std::string random_string(size_t length) {
	auto randchar = []() -> char
	{
		const char charset[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[rand() % max_index];
	};
	std::string str(length, 0);
	std::generate_n(str.begin(), length, randchar);
	return str;
}

#endif