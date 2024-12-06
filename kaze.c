/*
	Kaze (風) - a simple text editor made with C.
*/

/*** INCLUDES ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** DATA ***/

struct termios org_term;

/*** TERMINAL ***/

void die(const char *error)
{
	perror(error);
	exit(1);
}

void exitRawMode() 
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &org_term) == -1) 
		die("exitRawMode");
}

void enterRawMode()
{
	if (tcgetattr(STDIN_FILENO, &org_term) == -1)
		die("tcgetattr");
	atexit(exitRawMode);

	// Copy original terminal attrs and modify them
	struct termios raw_term = org_term;
	raw_term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw_term.c_oflag &= ~(OPOST);
	raw_term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw_term.c_cflag |= (CS8);
	raw_term.c_cc[VMIN] = 0;
	raw_term.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_term) == -1)
		die("tcsetattr");
}

/*** INIT ***/

int main() 
{
	enterRawMode();

	while (1) {
		char c = '\0';
		if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
			die("read");
		if (iscntrl(c)) {
			printf("%d\r\n", c);
		} else {
			printf("%d ('%c')\r\n", c, c);
		}
		if (c == 'q') {
			break;
		}
	}

	//
	return 0;
}