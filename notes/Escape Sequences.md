# Escape Sequences
Escape sequences are used to control the terminal display, allowing the program to move the cursor, clear parts of the screen, change text color, and more. They start with the ASCII Escape character (`\x1b`) followed by a series of characters.

## Here are some of them that I used:
- Clear screen: ``\ESC[2J``
- Clear line to the right: ``\ESC[K``
- Move cursor to top-left (home): ``\ESC[H``
- Move cursor to bottom-right: ``\ESC[999B\ESC[999C``
- Hide cursor: ``\ESC[?25l``
- Show cursor ``\ESC[?25h``
- Get cursor position: ``\ESC[6n``
- Red color: ``\ESC[31m``
- Green color: ``\ESC[32m``
- Yellow color: ``\ESC[33m``
- Reset text format: ``\ESC[0m``
- Bold text: ``\ESC[1m``
- Underline text: ``\ESC[4m``

## [VT100](https://en.wikipedia.org/wiki/VT100)