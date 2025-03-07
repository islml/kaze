package editor

import (
	"os"

	"golang.org/x/sys/unix"
)

type Term struct {
	OrigTerm unix.Termios
}

func (t *Term) EnterRawMode() error {
	rawTerm, err := unix.IoctlGetTermios(int(os.Stdin.Fd()), unix.TCSETS)
	if err != nil {
		return err
	}
	
	t.OrigTerm = *rawTerm

	rawTerm.Lflag &^= (unix.ECHO | unix.ICANON | unix.IEXTEN | unix.ISIG)
	rawTerm.Iflag &^= (unix.ICRNL | unix.INPCK | unix.BRKINT | unix.ISTRIP | unix.IXON)
	rawTerm.Oflag &^= (unix.OPOST)
	rawTerm.Cflag |= (unix.CS8)
	rawTerm.Cc[unix.VMIN] = 0
	rawTerm.Cc[unix.VTIME] = 100

	return unix.IoctlSetTermios(int(os.Stdin.Fd()), unix.TCSETS, rawTerm)
}

func (t *Term) ExitRawMode() error {
	return unix.IoctlSetTermios(int(os.Stdin.Fd()), unix.TCSETS, &t.OrigTerm)
}