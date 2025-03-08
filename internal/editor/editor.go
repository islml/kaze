package editor

import (
	"fmt"
	"io"
	"log"
	"os"
	"syscall"
	"unicode"
)

type Editor struct {
	Term Term
	ScreenRows uint16
	ScreenCols uint16
}

func (e *Editor) Init() {
	err := e.Term.EnterRawMode() 
	if err != nil {
		log.Fatalf("Error: %v", err)
	}  

	e.ScreenRows, e.ScreenCols, err = e.Term.GetWindowSize()
	if err != nil {
		log.Fatalf("Error: %v", err)
	}
}

func (e *Editor) ReadKey() (rune, error) {
	buf := make([]byte, 1)
	for {
		n, err := os.Stdin.Read(buf)
		if err != nil {
			if err == syscall.EAGAIN {
				continue
			}
			if err == io.EOF {
				return 0, nil
			}
			return 0, err
		}

		if n == 1 {
			return rune(buf[0]), nil
		}
	}
}

func (e *Editor) MapKey() {
	c, err := e.ReadKey()
	if err != nil {
		log.Fatalf("Error: %v", err)
	}

	switch c {
	case 0: // EOF = 0
		return
	case (('q') & (0x1F)):
		os.Exit(0)
	default:
		if unicode.IsControl(c) {
			fmt.Printf("%d\r\n", c)
		} else {
			fmt.Printf("%d ('%c')\r\n", c, c)
		}
	}
}