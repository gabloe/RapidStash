CXX=g++
INC=-IMemoryMappedFile/ -IFilesystem/ -ILogging/ -pthread
OPT=-std=c++11 -O3 -g -Wall -Wextra
OUT=build/
OBJ=build/obj/

testing: $(OUT) $(OBJ) filesystem filereader filewriter memorymappedfile
	$(CXX) $(OPT) $(INC) $(OBJ)Filesystem.o $(OBJ)DynamicMemoryMappedFile.o $(OBJ)Filereader.o $(OBJ)Filewriter.o ./Testing/TestFilesystem.cpp -o $(OUT)/Testing

filesystem: memorymappedfile
	$(CXX) $(OPT) $(INC) -c ./Filesystem/Filesystem.cpp -o $(OBJ)Filesystem.o

filereader: filesystem
	$(CXX) $(OPT) $(INC) -c ./Filesystem/Filereader.cpp -o $(OBJ)Filereader.o

filewriter: filesystem
	$(CXX) $(OPT) $(INC) -c ./Filesystem/Filewriter.cpp -o $(OBJ)Filewriter.o

memorymappedfile:
	$(CXX) $(OPT) $(INC) -c ./MemoryMappedFile/DynamicMemoryMappedFile.cpp -o $(OBJ)DynamicMemoryMappedFile.o

$(OUT):
	mkdir -p $(OUT)/obj

clean:
	rm -rf $(OUT)
