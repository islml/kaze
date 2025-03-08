package editor

import (
	"fmt"
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
	// rawTerm.Cc[unix.VMIN] = 0
	// rawTerm.Cc[unix.VTIME] = 10

	return unix.IoctlSetTermios(int(os.Stdin.Fd()), unix.TCSETS, rawTerm)
}

func (t *Term) ExitRawMode() error {
	return unix.IoctlSetTermios(int(os.Stdin.Fd()), unix.TCSETS, &t.OrigTerm)
}

func (t *Term) GetWindowSize() (uint16, uint16, error) {
	winsize, err := unix.IoctlGetWinsize(int(os.Stdin.Fd()), unix.TIOCGWINSZ)
	if err != nil {
		return 0, 0, err
	}
	return winsize.Row, winsize.Col, nil
}

func (t *Term) RefreshTerminal() error {
	if _, err := fmt.Fprint(os.Stdout, "\033[2j\033[H"); err != nil {
		return err
	}
	return nil
}