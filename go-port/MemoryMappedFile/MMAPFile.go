package filesystem

import (
	"assert"
	"bytes"
	"encoding/binary"
	"errors"
	"github.com/edsrzf/mmap-go"
	"jeff/math"
	"log"
	"os"
	"sync"
	"unsafe"
)

var _Sanity = []byte{0x0, 0x0, 0xd, 0x1, 0xe, 0x5, 0x0, 0xf, 0xd, 0x0, 0x0, 0xd, 0xa, 0xd, 0x5}

const (
	_Version      byte = byte(1)
	_MaxFileSize       = 1000000000
	_HeaderSize       = int(unsafe.Sizeof(_Version)+unsafe.Sizeof(1)) + 16
	_InitialSize      = 4096 + _HeaderSize
)

// File is an interface to represent basic operations of a file?
type File interface {
	Close() error
	Write(data []byte, pos int) (int, error)
	Read(pos, len, offset int) ([]byte, error)
	IsNew() bool
	Name() string
}

/* Implementation */

type header struct {
	sanity []byte
	ver    byte
	mSize  int64
}

type mmapFileImpl struct {
	memmap  mmap.MMap // mFile is just []byte
	mapSize int
	newFile bool
	file    *os.File
	lock    *sync.Mutex
	name    string
}

/* Required for interface */

// Close cleans up all resources, flushes, and closes the 
// memory mapped file
func (mFile *mmapFileImpl) Close() error {
	mFile.writeHeader()
	mFile.memmap.Flush()
	err := mFile.memmap.Unmap()

	if err != nil {
		return err
	}

	err = mFile.file.Close()
	if err != nil {
		return err
	}

	return nil
}

func (mFile *mmapFileImpl) Write(data []byte, pos int) (int, error) {
	start := pos + _HeaderSize
	end := start + len(data)

	mFile.lock.Lock()
	if end > mFile.mapSize {
		mFile.grow(end + _HeaderSize)
	}

	to := mFile.memmap[start:]
	if len(to) < len(data) {
		log.Fatal("Not enough space, didn't we grow?")
	}

	length := copy(to, data)
	mFile.lock.Unlock()
	mFile.memmap.Flush()

	return length, nil
}

func (mFile *mmapFileImpl) Read(pos, length, offset int) ([]byte, error) {
	start := _HeaderSize + pos + offset
	end := start + length

	if end > mFile.mapSize {
		return nil, errors.New("Tried to read beyond end of file")
	}

	if length > _MaxFileSize {
		log.Fatal("File too large")
	}

	result := make([]byte, length)
	if result == nil {
		log.Fatal("Could not allocate memory for read")
	}

	check := copy(result, mFile.memmap[start:])

	if check != length {
		log.Fatal("Could not read entire length")
	}

	return result, nil
}

// IsNew returns true if this file was new when created, otherwise
// returns false
func (mFile *mmapFileImpl) IsNew() bool {
	return mFile.newFile
}

// Name returns the filename
func (mFile *mmapFileImpl) Name() string {
	return mFile.name
}

/* Required to work */

func (mFile *mmapFileImpl) grow(newSize int) {
	// Get the new size
	mFile.mapSize = newSize

	// Flush and unmap
	mFile.memmap.Flush()
	mFile.memmap.Unmap()
	mFile.memmap = nil

	// Grow the file
	mFile.file.Truncate(int64(mFile.mapSize))

	var err error

	mFile.memmap, err = mmap.Map(mFile.file, mmap.RDWR, 0)

	if err != nil {
		log.Fatal("Could not resize file, handle gracefully later: ", err.Error())
	}

	if mFile.memmap == nil {
		log.Fatal("Could not resize file, handle gracefully later(2)")
	}

	if len(mFile.memmap) != mFile.mapSize {
		log.Fatal("Backing mapped array not same size")
	}

}

func (mFile *mmapFileImpl) writeHeader() {

	var buff bytes.Buffer
	err := binary.Write(&buff, binary.BigEndian,_Sanity)
	if err != nil {
		log.Fatal(err.Error())
	}

	err = binary.Write(&buff, binary.BigEndian, _Version)
	if err != nil {
		log.Fatal(err.Error())
	}

	err = binary.Write(&buff, binary.BigEndian, int64(mFile.mapSize))
	if err != nil {
		log.Fatal(err.Error())
	}
	copy(mFile.memmap, buff.Bytes())
	mFile.memmap.Flush()
}

func (mFile *mmapFileImpl) readHeader() header {
	var result header
	buff := bytes.NewBuffer(mFile.memmap[:_HeaderSize])
	result.sanity = make([]byte, 15)

	binary.Read(buff, binary.BigEndian, &result.sanity)
	binary.Read(buff, binary.BigEndian, &result.ver)
	binary.Read(buff, binary.BigEndian, &result.mSize)

	return result
}

func (mFile *mmapFileImpl) sanityCheck(h header) bool {
	return true
}

func (mFile *mmapFileImpl) align(offset int) int {
	const alignment = 16
	rem := offset % alignment
	return offset + alignment - rem
}

/* Constructors */

// NewFile creates a new memory mapped file
func NewFile(fName string) (File, error) {
	var err error

	result := new(mmapFileImpl)
	result.name = fName

	// Create/Open file
	result.file, err = os.OpenFile(fName, os.O_CREATE|os.O_RDWR, 0644)
	if err != nil {
		return nil, errors.New("Could not open file for reading")
	}

	// Check to see if new
	info, _ := os.Stat(fName)
	result.mapSize = math.MaxInt(_InitialSize, int(info.Size()))

	if info.Size() == 0 {
		result.newFile = true
		result.file.Truncate(int64(result.mapSize))
	} else {
		result.newFile = false
	}

	// Map file to memory
	result.memmap, err = mmap.Map(result.file, mmap.RDWR, 0)

	// Validate
	if err != nil {
		return nil, err
	}

	result.lock = &sync.Mutex{}

	if !result.newFile {

		head := result.readHeader()

		assert.Assert(bytes.Equal(head.sanity, _Sanity), "Sanity check failed '"+string(head.sanity)+"'")
		assert.Assert(head.ver == _Version, "Versions do not match: ", head.ver, " vs. ", _Version)
		assert.Assert(int(head.mSize) == result.mapSize, fName+": Sizes do not match: ", head.mSize, result.mapSize)

	} else {
		result.writeHeader()
	}
	return result, nil
}
