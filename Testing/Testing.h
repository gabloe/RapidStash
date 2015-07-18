#ifndef _TESTING_H_
#define _TESTING_H_

#pragma once
#include <functional>
#include <iostream>
#include <string>
#include <Filesystem.h>

int TestReadWrite(STORAGE::Filesystem *);
int TestConcurrentWrite(STORAGE::Filesystem *);

typedef std::function<void()> TestWrapper_t;

static void TestWrapper(std::string name, STORAGE::Filesystem *fs, std::function<int(STORAGE::Filesystem*)> fn) {
	std::cout << name << ": ";
	int res = fn(fs);
	if (res == 0) {
		std::cout << "PASSED!";
	} else {
		std::cout << "FAILED!";
	}
	std::cout << "\n";
}

#endif