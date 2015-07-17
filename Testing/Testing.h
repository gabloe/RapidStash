#ifndef _TESTING_H_
#define _TESTING_H_

#pragma once
#include <functional>
#include <iostream>
#include <string>

int TestReadWrite();
int TestConcurrentWrite();

typedef std::function<void()> TestWrapper_t;

static void TestWrapper(std::string name, std::function<int()> fn) {
	std::cout << name << ": ";
	int res = fn();
	if (res == 0) {
		std::cout << "PASSED!";
	} else {
		std::cout << "FAILED!";
	}
	std::cout << "\n";
}

#endif