
#include <sstream>
#include <vector>
#include <iostream>
#include <string>
#include <gason.h>

#include <FileSystem.h>

#include <chrono>

void performance(std::string& msg) {
	const int iterations = 100000;
	char* herp = new char[1024*1024];
	char* endptr = nullptr;

	JsonAllocator allocator;

	std::cout << "Parsing: " << msg << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < iterations; ++i) {
		memcpy(herp, msg.c_str(), msg.length() + 1);
		JsonValue value;

		int status = jsonParse(herp, &endptr, &value, allocator);
		if (status != JSON_OK) {
			std::cout << "Failure?" << std::endl;
		}
		allocator.deallocate();
	}
	delete herp;
	auto end = std::chrono::high_resolution_clock::now();

	std::cout << iterations  / (0.000001 + std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count())<< " iterations per millisecond" << std::endl;
 }

void fileSystemPerformance(STORAGE::Filesystem* fs, std::string filename, std::string should) {
	const int iterations = 100000;
	char* herp = new char[1024 * 1024];
	char* endptr = nullptr;

	JsonAllocator allocator;


	std::cout << "Parsing: " << should << std::endl;
	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < iterations; ++i) {
		File file = fs->select(filename);
		STORAGE::IO::Reader reader = fs->getReader(file);
		std::string msg = reader.readString();
		memcpy(herp, msg.c_str(), msg.length() + 1);
		JsonValue value;

		int status = jsonParse(herp, &endptr, &value, allocator);
		if (status != JSON_OK) {
			std::cout << "Failure?" << std::endl;
		}
		allocator.deallocate();
	}
	delete herp;
	auto end = std::chrono::high_resolution_clock::now();

	std::cout << iterations / (0.000001 + std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) << " iterations per millisecond" << std::endl;
}

int main(int argc, char* argv[]) {
	
	std::vector<std::string> tests;

	// Linear JSON of numbers
	tests.push_back("{\"a\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1,\"f\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1,\"f\" : 1,\"g\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1,\"f\" : 1, \"g\" : 1, \"h\" : 1}");
	tests.push_back("{\"a\" : 1,\"b\" : 1,\"c\" : 1,\"d\" : 1,\"e\" : 1,\"f\" : 1, \"g\" : 1, \"h\" : 1, \"i\" : 1}");

	// Linear JSON of strings
	tests.push_back("{\"a\" : \"1\"}");
	tests.push_back("{\"a\" : \"1\", \"b\" : \"1\"}");
	tests.push_back("{\"a\" : 1,\"b\" : \"1\",\"c\" : \"1\"}");
	tests.push_back("{\"a\" : 1,\"b\" : \"1\",\"c\" : \"1\",\"d\" : \"1\"}");
	tests.push_back("{\"a\" : 1,\"b\" : \"1\",\"c\" : \"1\",\"d\" : \"1\",\"e\" : \"1\"}");
	tests.push_back("{\"a\" : 1,\"b\" : \"1\",\"c\" : \"1\",\"d\" : \"1\",\"e\" : \"1\",\"f\" : \"1\"}");
	tests.push_back("{\"a\" : 1,\"b\" : \"1\",\"c\" : \"1\",\"d\" : \"1\",\"e\" : \"1\",\"f\" : \"1\",\"g\" : \"1\"}");
	tests.push_back("{\"a\" : 1,\"b\" : \"1\",\"c\" : \"1\",\"d\" : \"1\",\"e\" : \"1\",\"f\" : \"1\", \"g\" : \"1\", \"h\" : \"1\"}");
	tests.push_back("{\"a\" : 1,\"b\" : \"1\",\"c\" : \"1\",\"d\" : \"1\",\"e\" : \"1\",\"f\" : \"1\", \"g\" : \"1\", \"h\" : \"1\", \"i\" : \"1\"}");


	// Recursive JSON objects
	tests.push_back("{ \"a\" : { \"b\" : 5} }");
	tests.push_back("{ \"a\" : { \"b\" : { \"c\" : 5 } } }");
	tests.push_back("{ \"a\" : { \"b\" : { \"c\" : { \"d\" : 5 } } } }");

	// key : sub-JSON objects
	tests.push_back("{ \"a\" : { \"aa\" : 1 } }");
	tests.push_back("{ \"a\" : { \"aa\" : 1 } , \"b\" : { \"bb\" : 6 } }");
	tests.push_back("{ \"a\" : { \"aa\" : 1 } , \"b\" : { \"bb\" : 6 } , \"c\" : { \"cc\" : 5 } }");

	// Array
	tests.push_back(" \"a\" : [ 1 ]");
	tests.push_back(" \"a\" : [ 1 2 ]");
	tests.push_back(" \"a\" : [ 1 2 3 ]");
	tests.push_back(" \"a\" : [ 1 2 3 4 ]");
	tests.push_back(" \"a\" : [ 1 2 3 4 5 ]");
	tests.push_back(" \"a\" : [ 1 2 3 4 5 6 ]");

	// Array with commas
	tests.push_back(" \"a\" : [ 1 ]");
	tests.push_back(" \"a\" : [ 1, 2 ]");
	tests.push_back(" \"a\" : [ 1, 2, 3 ]");
	tests.push_back(" \"a\" : [ 1, 2, 3, 4 ]");
	tests.push_back(" \"a\" : [ 1, 2, 3, 4, 5 ]");
	tests.push_back(" \"a\" : [ 1, 2, 3, 4, 5, 6 ]");

	// Arrays of arrays
	tests.push_back(" \"a\" : [ [ 1 ] ]");
	tests.push_back(" \"a\" : [  [ 1 ] , [ 2 ] ]");
	tests.push_back(" \"a\" : [ [1], [2], [3] ]");
	tests.push_back(" \"a\" : [ [1], [2], [3], [4] ]");
	tests.push_back(" \"a\" : [ [1], [2], [3], [4] , [5] ]");
	tests.push_back(" \"a\" : [ [1], [2], [3], [4] , [5] , [6] ]");
	
	std::stringstream ss;
	
	const int TerribleLength = 200;

	ss << "{";
	for (int i = 1; i < TerribleLength; ++i) {
		ss << "\"a" << i << "\":" << i << ",";
	}
	ss << "\"a" << TerribleLength << "\":" << TerribleLength << "}";

	tests.push_back(ss.str());

	STORAGE::Filesystem fs("derp.txt");

	for (int i = 0; i < tests.size(); ++i) {
		std::string* ref = &(tests[i]);

		std::string fName = "test" + toString(i);
		File file;
		if (!fs.exists(fName)) {
			std::cout << "Creating" << std::endl;
			file = fs.select(fName);
			STORAGE::IO::Writer writer = fs.getWriter(file);
			writer.write(ref->c_str(), ref->length() + 1);
		}
	}

	for (int i = 0; i < tests.size(); ++i) {
		std::string fName = "test" + toString(i);
		std::string ref = (tests[i]);
		fileSystemPerformance(&fs, fName, ref);
	}



	fs.shutdown();
	std::cin.get();
	return 0;
}