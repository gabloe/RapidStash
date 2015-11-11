#ifndef _TESTING_H_
#define _TESTING_H_

#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <Filesystem.h>

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

static const int numNames = 16;
static const int dataSize = 4096;
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

enum COLOR {
	RED,
	BLUE,
	GREEN,
	WHITE
};

static void setConsoleColor(COLOR color) {
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	switch (color) {
	case RED: SetConsoleTextAttribute(hConsole, FOREGROUND_RED); break;
	case GREEN: SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN); break;
	case BLUE: SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE); break;
	default: SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); break;
	}
#else
	switch (color) {
	case RED: std::cout << "\033[31m"; break;
	case GREEN: std::cout << "\033[32m"; break;
	case BLUE: std::cout << "\033[34m"; break;
	default: std::cout << "\033[0m"; break;
	}
#endif
}

static void TestWrapper(std::string name, std::function<int(STORAGE::Filesystem*)> fn) {
	setConsoleColor(WHITE);
	std::cout << std::setw(30) << std::right << name << ": ";
	std::string fname("data/" + name);
	STORAGE::Filesystem *fs = new STORAGE::Filesystem(fname.c_str());
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
	int res = fn(fs);
	std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
	if (res == 0) {
		setConsoleColor(GREEN);
		std::cout << std::left << "PASSED!";
	} else {
		setConsoleColor(RED);
		std::cout << std::left << "FAILED!";
	}
	setConsoleColor(WHITE);
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