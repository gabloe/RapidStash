
#include <iostream>
#include <string>
#include <gason.h>

#include <chrono>

void performance(char* msg) {
	const int iterations = 100000;
	char herp[1024];
	char* endptr = nullptr;
	std::cout << "Parsing: " << herp << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < iterations; ++i) {
		strcpy_s(herp, msg);
		JsonAllocator allocator;
		JsonValue value;

		int status = jsonParse(herp, &endptr, &value, allocator);
		if (status != JSON_OK) {
			std::cout << "Failure?" << std::endl;
		}
	}
	auto end = std::chrono::high_resolution_clock::now();

	std::cout << iterations  / (0.000001 + std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count())<< " iterations per millisecond" << std::endl;
 }

int main(int argc, char* argv[]) {
	if (argc != 2) return 0;
	performance(argv[1]);
	std::cin.get();
	return 0;
}