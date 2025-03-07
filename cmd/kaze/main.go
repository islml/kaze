package main

import (
	"fmt"
	"os"
	"unicode"

	"github.com/islml/kaze/internal/editor"
)

func main() {
	var kaze editor.Term
	kaze.EnterRawMode()
	defer kaze.ExitRawMode()

	for {
		buf := make([]byte, 1)
		os.Stdin.Read(buf)

		if unicode.IsControl(rune(buf[0])) {
			fmt.Printf("%d\r\n", rune(buf[0]))
		} else {
			fmt.Printf("%d ('%c')\r\n", rune(buf[0]), rune(buf[0]))
		}

		if rune(buf[0]) == 'q' {
			break
		}
	}
}