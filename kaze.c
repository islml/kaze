/*
	Kaze (風) - a simple text editor made with C.
*/

/*** INCLUDES ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/

// Some Esc sequences - with their sizes
#define ESC(seq)     		"\x1b" seq 
#define SCREEN_CLEAR 		ESC("[2J") 				  	// 4
#define CURSOR_HOME  	 	ESC("[H")  					// 3
#define CURSOR_BOTTOM_RIGHT ESC("[999B") ESC("[999C")	// 12
#define CURSOR_POSITION		ESC("[6n")					// 4
#define CURSOR_UP    		ESC("[A")  					// 3
#define CURSOR_DOWN  		ESC("[B")  					// 3
#define CURSOR_RIGHT 		ESC("[C")  					// 3
#define CURSOR_LEFT  		ESC("[D")  					// 3

#define CTRL_KEY(k) ((k) & (0x1F))

/*** DATA ***/

struct editorConfiguration {
	struct termios org_term;

	int screenrows;
	int screencols;

};

struct editorConfiguration E;

/*** TERMINAL ***/

void die(const char *error)
{
	write(STDOUT_FILENO, SCREEN_CLEAR, 4);
	write(STDOUT_FILENO, CURSOR_HOME, 3);

	perror(error);
	exit(1);
}

void exitRawMode() 
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.org_term) == -1) 
		die("exitRawMode");
}

void enterRawMode()
{
	if (tcgetattr(STDIN_FILENO, &E.org_term) == -1)
		die("tcgetattr");
	atexit(exitRawMode);

	// Copy original terminal attrs and modify them
	struct termios raw_term = E.org_term;
	raw_term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw_term.c_oflag &= ~(OPOST);
	raw_term.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw_term.c_cflag |= (CS8);
	raw_term.c_cc[VMIN] = 0;
	raw_term.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_term) == -1)
		die("tcsetattr");
}

char editorReadKey()
{
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) { 
		if (nread == -1 && errno != EAGAIN) 
			die("read");
	}
	return c;
}

int getCursorPos(int *screenrows, int *screencols)
{
	char buf[32];
	if (write(STDOUT_FILENO, CURSOR_POSITION, 4) != 4)
		return -1;
	
	for (unsigned int i = 0; i < sizeof(buf) - 1; i++) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1 || buf[i] == 'R') {
			buf[i] = '\0';
			break;
		}
	}

	if (buf[0] != '\x1b' || buf[1] != '[')
		return -1;
	if (sscanf(&buf[1], "[%d;%d", screenrows, screencols) != 2)
		return -1;
	
	return 0;
}

int getWinSize(int *screenrows, int *screencols)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == - 1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, CURSOR_BOTTOM_RIGHT, 12) != 12)
			return -1;
		return getCursorPos(screenrows, screencols);
	} else {
		*screenrows = ws.ws_row;
		*screenrows = ws.ws_col;
		return 0;
	}
}

/*** OUTPUT ***/

void editorDrawRows()
{
	for (int y = 0; y < E.screenrows; y++) {
		write(STDOUT_FILENO, "~", 1);

		if (y != E.screenrows) 
			write(STDOUT_FILENO, "\r\n", 2);
	}
}

void editorResfreshScreen()
{
	write(STDOUT_FILENO, SCREEN_CLEAR,4);
	write(STDOUT_FILENO, CURSOR_HOME, 3);

	editorDrawRows();
	write(STDOUT_FILENO, CURSOR_HOME, 3);
}

/*** INPUT ***/

void editorMapKeypress()
{
	char c = editorReadKey();

	switch (c) {
	case CTRL_KEY('q'):
		write(STDOUT_FILENO, SCREEN_CLEAR, 4);
		write (STDOUT_FILENO, CURSOR_HOME, 3);
		exit(0);
		break;
	}
}

/*** INIT ***/

void initEditor()
{
	if (getWinSize(&E.screenrows, &E.screencols) == -1)
		die("getWinSize");
}

int main() 
{
	enterRawMode();
	initEditor();

	while (1) {
		editorResfreshScreen();
		editorMapKeypress();
	}

	//
	return 0;
}