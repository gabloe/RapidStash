package mmapfile

import (
	"errors"
	"github.com/edsrzf/mmap-go"
	"log"
	"os"
)

const (
	VERSION      = 1
	MaxFileSize  = 1000000000
	INITIAL_SIZE = 4098
)

var SANITY = []byte{0x0, 0x0, 0xd, 0x1, 0xe, 0x5, 0x0, 0xf, 0xd, 0x0, 0x0, 0xd, 0xa, 0xd, 0x5}

type MMAPFile interface {
	Close()
	Write()
	Read(pos, len, offset int) ([]byte, error)
	IsNew() bool
	Name() string
}

/* Implementation */

type header struct {
}

type MMAPFileImpl struct {
	memmap   mmap.MMap // This is just []byte
	numPages int
	mapSize  int
	fHandle  interface{}
	newFile  bool
	file     *os.File
}

/* Required for interface */

func (this *MMAPFileImpl) Close() {
}

func (this *MMAPFileImpl) Write() {
}

func (this *MMAPFileImpl) Read(pos, len, offset int) ([]byte, error) {
	start := pos + offset
	end := start + len

	if end > this.mapSize {
		log.Fatal("Tried to read beyond end of file")
		return nil, errors.New("Tried to read beyond end of file")
	}

	if len > MaxFileSize {
		log.Fatal("File too large")
	}

	result := make([]byte, len)
	if result == nil {
		log.Fatal("Could not allocate memory for read")
	}

	check := copy(result, this.memmap[start:len])
	if check != len {
		log.Fatal("Could not read entire length")
	}

	return result, nil
}

func (this *MMAPFileImpl) IsNew() bool {
	return this.newFile
}

func (this *MMAPFileImpl) Name() string {
	return this.file.Name()
}

/* Required to work */

func (this *MMAPFileImpl) grow(newSize int) {
}

func (this *MMAPFileImpl) writeHeader() {
	log.Print("Writing header for:", this.Name())

	this.memmap.Flush()
}

func (this *MMAPFileImpl) readHeader() header {
	var result header
	return result
}

func (this *MMAPFileImpl) sanityCheck(h header) bool {
	return true
}

func (this *MMAPFileImpl) align(offset int) {
}

/* Constructors */

func NewFile(fName string) MMAPFile {
	result := new(MMAPFileImpl)

	var err error

	result.file, err = os.OpenFile(fName, os.O_CREATE|os.O_RDWR, 0644)

	info, _ := os.Stat(fName)
	result.mapSize = info.Size()

	if result.mapSize == 0 {
		log.Print("Creating new file", fName)
		result.mapSize = INITIAL_SIZE
		result.file.Truncate(int64(result.mapSize))
		result.writeHeader()
	} else {
		info, _ := os.Stat(fName)
		result.mapSize = int(info.Size())
	}

	result.memmap, err = mmap.Map(result.file, mmap.RDWR, 0)

	if err != nil {
		log.Fatal(err)
	}

	return result
}
