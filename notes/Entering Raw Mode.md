The terminal is in canonical (cooked) mode by default, processing input by buffering until pressing ``Enter`` or ``EOF``. Raw mode disables this, turning off input buffering, special character handling, and other default terminal behaviors by modifying specific terminal flags. 
To Enter raw mode in C language we will use the [``termios.h``](https://pubs.opengroup.org/onlinepubs/7908799/xsh/termios.h.html) header file by disabling the following features and enabling the last one.

## Input flags (``c_iflag``):
- BRKINT - Signal interrupt on break, (``ctrl + c``).
- ICRNL - Map CR to NL on input, (``\r\n``).
- INPCK - Enable input parity check.
- ISTRIP - Strip charachter.
- IXON - Enable start/stop output control, (``ctrl + s``, ``ctrl + q``).

## Output flags (``c_oflag``):
- OPOST - Post-process output.

## Local flags (``c_lflag``):
- ECHO - Printing your input into the terminal.
- ICANON - Canonical input (editing the line before sending it).
- IEXTEN - Extending input character processing.
- ISIG - Enable signals, (``ctrl + c``, ``ctrl + z``).

## Control flags (``c_cflag``):
- CS8 - Set the character size to 8 bits.