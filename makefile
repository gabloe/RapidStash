CXX=g++
INC=-IMemoryMappedFile/ -IFilesystem/ -ILogging/ -pthread
OPT=-std=c++11 -O3 -g -Wall -Wextra
OUT=build/
OBJ=build/obj/

testing: $(OUT) $(OBJ) filesystem memorymappedfile
	$(CXX) $(OPT) $(INC) $(OBJ)Filesystem.o $(OBJ)DynamicMemoryMappedFile.o $(OBJ)common.o ./Testing/TestFilesystem.cpp -o $(OUT)/Testing

filesystem: memorymappedfile
	$(CXX) $(OPT) $(INC) -c ./Filesystem/Filesystem.cpp -o $(OBJ)Filesystem.o

memorymappedfile: common
	$(CXX) $(OPT) $(INC) -c ./MemoryMappedFile/DynamicMemoryMappedFile.cpp -o $(OBJ)DynamicMemoryMappedFile.o

common:
	$(CXX) $(OPT) $(INC) -c ./MemoryMappedFile/common.cpp -o $(OBJ)common.o

$(OUT):
	mkdir -p $(OUT)/obj

clean:
	rm -rf $(OUT)
