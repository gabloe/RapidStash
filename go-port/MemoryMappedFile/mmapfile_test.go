package mmapfile

import (
	"bytes"
	"log"
	"os"
	"path/filepath"
	"testing"
)

const (
	LargeFile = 1024 * 1024
)

var testData = []byte("asdfgasdfgasdfgasdfgasdfgasdfg")
var testPath = filepath.Join(os.TempDir(), "testdata")

type wrapper struct {
	derp *testing.T
}

func init() {
	os.Remove(testPath)
	log.SetFlags(log.Ltime | log.LstdFlags | log.Lshortfile)
}

func TestCreate(t *testing.T) {

	file, err := NewFile(testPath)
	if err != nil {
		t.Error(err.Error())
		return
	}
	file.Close()

	info, err := os.Stat(testPath)
	if err != nil {
		t.Error("Could not create file")
		return
	}

	if info.Size() != int64(INITIAL_SIZE) {
		t.Error("Incorrect initial size")
		return
	}

	file.Close()
}

func TestWrite(t *testing.T) {

	file, err := NewFile(testPath)
	if err != nil {
		t.Error(err.Error())
		return
	}
	file.Write(testData, 0)

	data, err := file.Read(0, len(testData), 0)

	if err != nil {
		t.Error(err.Error())
	}

	same := bytes.Equal(testData, data)
	if !same {
		t.Error("Data not the same")
	}

	file.Close()

}

func TestRead(t *testing.T) {
	file, err := NewFile(testPath)

	if err != nil {
		t.Error(err.Error())
		return
	}

	data, err := file.Read(0, len(testData), 0)

	if err != nil {
		t.Error(err.Error())
	}

	same := bytes.Equal(testData, data)
	if !same {
		t.Error("Data not the same")
	}

	file.Close()
}

func TestWriteLargeFile(t *testing.T) {
	data := make([]byte, LargeFile)
	for i := 0; i < LargeFile; i++ {
		data[i] = byte(i % 256)
	}

	file, err := NewFile(testPath)

	if err != nil {
		t.Error(err.Error())
		return
	}

	file.Write(data, 0)
	file.Close()
}

func TestReadLargeFile(t *testing.T) {
	data := make([]byte, LargeFile)
	for i := 0; i < LargeFile; i++ {
		data[i] = byte(i % 256)
	}

	file, err := NewFile(testPath)

	if err != nil {
		t.Error(err.Error())
		return
	}

	test, err := file.Read(0, len(data), 0)
	if err != nil {
		t.Error(err.Error())
		return
	}
	file.Close()

	if !bytes.Equal(data, test) {
		t.Error("Data not correct")
		return
	}
}

func TestTearDown(t *testing.T) {
	os.Remove(testPath)
	info, err := os.Stat(testPath)
	if err == nil {
		t.Error("File still exists: ", testPath)
	}

	if info != nil {
		t.Error("File still exists")
	}
}
