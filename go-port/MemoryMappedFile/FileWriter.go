package filesystem

import (
	"io"
)

type mmapWriter struct {
	file File
}

func (this *mmapWriter) Write(data []byte) (written int, err error) {
	this.file.Write(data, 0)
	return len(data), nil
}

func NewWriter(f File) io.Writer {
	return new(mmapWriter)
}
