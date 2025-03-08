package main

import (
  "github.com/islml/kaze/internal/editor"
)

func main() {
  var kaze editor.Editor
  kaze.Init()

  for {             
    kaze.MapKey()
  }
}