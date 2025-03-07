package main

import (
  "fmt"
  "log"
  "os"
  "unicode"

  "github.com/islml/kaze/internal/editor"
)

func main() {
  var kaze editor.Term
  if err := kaze.EnterRawMode(); err != nil {
    log.Fatalf("Error: %v", err)
  }
  defer kaze.ExitRawMode()

  buf := make([]byte, 1)
  for {             
    _, err := os.Stdin.Read(buf)
    if err != nil {
      log.Fatalf("Error: %v", err)
    }

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