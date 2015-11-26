
package filesystem

import (
	"io"
)

// FileSystem does something
type FileSystem interface {
	GetWriter() io.Writer
}

// OpenFileSystem is the best FileSystem
func OpenFileSystem(filename string) FileSystem {
	return nil
}