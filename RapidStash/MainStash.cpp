
#include <vector>
#include <iostream>
#include <string>
#include <gason.h>

#include <chrono>

void performance(std::string msg) {
	const int iterations = 100000;
	char herp[1024];
	char* endptr = nullptr;
	std::cout << "Parsing: " << msg << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < iterations; ++i) {
		strcpy_s(herp, msg.c_str());
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
	
	std::vector<std::string> tests;

	tests.push_back("{\"a\" : 1,}");
	tests.push_back("{\"a\" : 1,\"b\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1,\"f\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1,\"f\" : 1,\"g\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1,\"f\" : 1, \"g\" : 1, \"h\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1,\"f\" : 1, \"g\" : 1, \"h\" : 1, \"i\" : 1}");

	for (int i = 0; i < tests.size(); ++i) {
		performance(tests[i]);
	}
	std::cin.get();
	return 0;
}