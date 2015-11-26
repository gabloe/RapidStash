package filesystem

import (
	"io"
)

type mmapWriter struct {
	file File
}

func (writer *mmapWriter) Write(data []byte) (written int, err error) {
	return writer.file.Write(data, 0)
}


// NewWriter takes in a File object and returns a writer that
// allows users to Write to the file 
func NewWriter(f File) io.Writer {
	return new(mmapWriter)
}
