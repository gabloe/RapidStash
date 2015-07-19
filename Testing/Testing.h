#ifndef _TESTING_H_
#define _TESTING_H_

#pragma once
#include <functional>
#include <iostream>
#include <string>
#include <Filesystem.h>

int TestReadWrite(STORAGE::Filesystem *);
int TestConcurrentWrite(STORAGE::Filesystem *);
int TestHeader(STORAGE::Filesystem *);
int TestMVCC(STORAGE::Filesystem *);

typedef std::function<void()> TestWrapper_t;

static void TestWrapper(std::string name, std::function<int(STORAGE::Filesystem*)> fn) {
	std::cout << name << ": ";
	std::string fname("data/" + name);
	STORAGE::Filesystem *fs = new STORAGE::Filesystem(fname.c_str());
	int res = fn(fs);
	fs->shutdown();
	delete fs;
	if (res == 0) {
		std::cout << "PASSED!";
	} else {
		std::cout << "FAILED!";
	}
	std::cout << "\n";
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