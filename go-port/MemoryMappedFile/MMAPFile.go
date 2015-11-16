package mmapfile

import (
	"errors"
	"github.com/edsrzf/mmap-go"
	"log"
	"os"
	"sync"
	"unsafe"
)

var SANITY = []byte{0x0, 0x0, 0xd, 0x1, 0xe, 0x5, 0x0, 0xf, 0xd, 0x0, 0x0, 0xd, 0xa, 0xd, 0x5}

const (
	VERSION      = 1
	MaxFileSize  = 1000000000
	HEADER_SIZE  = int(unsafe.Sizeof(VERSION)+unsafe.Sizeof(1)) + 16
	INITIAL_SIZE = 4096 + HEADER_SIZE
)

type MMAPFile interface {
	Close() error
	Write(data []byte, pos int)
	Read(pos, len, offset int) ([]byte, error)
	IsNew() bool
	Name() string
}

/* Implementation */

type header struct {
}

type MMAPFileImpl struct {
	memmap  mmap.MMap // This is just []byte
	mapSize int
	newFile bool
	file    *os.File
	lock    *sync.Mutex
	name    string
}

/* Required for interface */

func (this *MMAPFileImpl) Close() error {
	this.memmap.Flush()
	err := this.memmap.Unmap()

	if err != nil {
		return err
	}

	err = this.file.Close()
	if err != nil {
		return err
	}

	return nil
}

func (this *MMAPFileImpl) Write(data []byte, pos int) {
	start := pos + HEADER_SIZE
	end := start + len(data)

	this.lock.Lock()
	if end > this.mapSize {
		this.grow(end + HEADER_SIZE)
	}

	to := this.memmap[start:]
	if len(to) < len(data) {
		log.Fatal("Not enough space, didn't we grow?")
	}

	copy(to, data)
	this.lock.Unlock()

	this.memmap.Flush()

}

func (this *MMAPFileImpl) Read(pos, length, offset int) ([]byte, error) {
	start := HEADER_SIZE + pos + offset
	end := start + length

	if end > this.mapSize {
		return nil, errors.New("Tried to read beyond end of file")
	}

	if length > MaxFileSize {
		log.Fatal("File too large")
	}

	result := make([]byte, length)
	if result == nil {
		log.Fatal("Could not allocate memory for read")
	}

	check := copy(result, this.memmap[start:])

	if check != length {
		log.Fatal("Could not read entire length")
	}

	return result, nil
}

func (this *MMAPFileImpl) IsNew() bool {
	return this.newFile
}

func (this *MMAPFileImpl) Name() string {
	return this.name
}

/* Required to work */

func (this *MMAPFileImpl) grow(newSize int) {
	// Get the new size
	this.mapSize = newSize

	// Flush and unmap
	this.memmap.Flush()
	this.memmap.Unmap()
	this.memmap = nil

	// Grow the file
	this.file.Truncate(int64(this.mapSize))

	var err error

	this.memmap, err = mmap.Map(this.file, mmap.RDWR, 0)

	if err != nil {
		log.Fatal("Could not resize file, handle gracefully later: ", err.Error())
	}

	if this.memmap == nil {
		log.Fatal("Could not resize file, handle gracefully later(2)")
	}

	if len(this.memmap) != this.mapSize {
		log.Fatal("Backing mapped array not same size")
	}

}

func (this *MMAPFileImpl) writeHeader() {
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

func NewFile(fName string) (MMAPFile, error) {

	result := new(MMAPFileImpl)
	result.memmap = nil
	result.file = nil
	result.name = ""
	result.mapSize = 0

	// Create/Open file
	file, err := os.OpenFile(fName, os.O_CREATE|os.O_RDWR, 0644)
	if err != nil {
		return nil, errors.New("Could not open file for reading")
	}

	// Check to see if new
	info, _ := os.Stat(fName)
	result.mapSize = int(info.Size())

	if info.Size() == 0 {
		result.newFile = true
		result.mapSize = INITIAL_SIZE
		file.Truncate(int64(result.mapSize))
		result.writeHeader()
	} else {
		result.newFile = false
		result.mapSize = int(info.Size())
	}

	// Map file to memory
	mapped, err := mmap.Map(file, mmap.RDWR, 0)

	// Validate
	if err != nil {
		return nil, err
	}

	// Setup struct
	result.memmap = mapped
	result.file = file
	result.name = fName
	result.lock = &sync.Mutex{}

	return result, nil
}
