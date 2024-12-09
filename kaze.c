/*
	Kaze (風) - a simple text editor made with C.
*/

/*** INCLUDES ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/

// Some Esc sequences - with their sizes
#define ESC 				'\x1b'
#define ESC_(seq) 			"\x1b" seq
#define CLEAR_SCREEN 		ESC_("[2J") 				// 4
#define CLEAR_LINE_RIGHT 	ESC_("[K") 					// 3
#define CURSOR_HOME 		ESC_("[H")					// 3
#define CURSOR_HIDE 		ESC_("[?25l") 				// 6
#define CURSOR_SHOW 		ESC_("[?25h") 				// 6
#define CURSOR_BOTTOM_RIGHT ESC_("[999B") ESC_("[999C") // 12
#define CURSOR_POSITION 	ESC_("[6n") 				// 4

// Some colors & and texts - with their sizes
#define RED 				ESC_("[31m")				// 5 
#define GREEN				ESC_("[32m") 				// 5
#define YELLOW				ESC_("[33m")				// 5
#define RESET 				ESC_("[0m")					// 4
#define BOLD 				ESC_("[1m")					// 4
#define UNDERLINE			ESC_("[4m") 				// 4

#define CTRL_KEY(k) 		((k) & (0x1F))
#define ABUFF_INIT 			{NULL, 0}
#define KAZE_VERSION		"0.0.1"

enum editorKeys {
	ARROW_UP  = 1000,
	ARROW_DOWN,
	ARROW_LEFT,
	ARROW_RIGHT,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

/*** DATA ***/

typedef struct erow {
	int size;
	char *chars;
} erow;

struct editorConfiguration {
	struct termios org_term;
	erow *row;
	int numrows;
	int screenrows;
	int screencols;
	int rowoff;
	int cx, cy;	// x - horizontal (cols), y - vertical(rows) 

};

struct editorConfiguration E;

/*** TERMINAL ***/

void die(const char *error)
{
	write(STDOUT_FILENO, CLEAR_SCREEN, 4);
	write(STDOUT_FILENO, CURSOR_HOME, 3);

	perror(error);
	exit(1);
}

void exitRawMode() 
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.org_term) == -1) 
		die(RED "exitRawMode" RESET);
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

/*
	Escape sequences for some special keys:
	- Arrow up    	: ESC + [ + A 
	- Arrow down  	: ESC + [ + B
	- Arrow right 	: ESC + [ + C
	- Arrow left  	: ESC + [ + D 
	- Page up     	: ESC + [ + 5 + ~
	- Page down   	: ESC + [ + 6 + ~
	- Home key    	: ESC + [ + 1 + ~
					: ESC + [ + 7 + ~
					: ESC + [ + H
					: ESC + [ + O + H
	- End key		: ESC + [ + 4 + ~
					: ESC + [ + 8 + ~
					: ESC + [ + F
					: ESC + [ + O + F
	- Delete key    : ESC + [ + 3 + ~
*/

int editorReadKey()
{
	int nread;
	char key;
	while ((nread = read(STDIN_FILENO, &key, 1)) != 1) { 
		if (nread == -1 && errno != EAGAIN) 
			die("read");
	}

	if (key == '\x1b') { // Check if key press == ESC
		char seq[3];
		if (read(STDIN_FILENO, &seq[0], 1) != 1)
			return ESC;
		if (read(STDIN_FILENO, &seq[1], 1) != 1)
			return ESC;
		
		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1) 
					return ESC;
				if (seq[2] == '~') {
					switch (seq[1]) {
						case '5':
							return PAGE_UP;
						case '6':
							return PAGE_DOWN;
						case '1':
						case '7':
							return HOME_KEY;
						case '4':
						case '8':
							return END_KEY;
						case '3':
							return DEL_KEY;
					}
				}
			} else {
				switch (seq[1]) {
					case 'A':
						return ARROW_UP;
					case 'B': 
						return ARROW_DOWN;
					case 'C':
						return ARROW_RIGHT;
					case 'D':
						return ARROW_LEFT;
					case 'H':
						return HOME_KEY;
					case 'F':
						return END_KEY;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
				case 'H':
					return HOME_KEY;
				case 'F':
					return END_KEY;
			}
		} else {
			return ESC;
		}
	} 
	
	return key;
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
		if (write(STDOUT_FILENO, CURSOR_BOTTOM_RIGHT, 13) != 13)
			return -1;
		return getCursorPos(screenrows, screencols);
	} else {
		*screenrows = ws.ws_row;
		*screencols = ws.ws_col;
		return 0;
	}
}

/*** ROW OPERATIONS ***/

void editorAppendRow(char *line, size_t linelen)
{
	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

	int idx = E.numrows;
	E.row[idx].size = linelen;
	E.row[idx].chars = malloc(linelen + 1);
	memcpy(E.row[idx].chars, line, linelen);
	E.row[idx].chars[linelen] = '\0';
	E.numrows++;
}

/*** FILE I/O ***/

void editorOpen(char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (!fp)
		die("fopen");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\n'))
			linelen--;
		
		editorAppendRow(line, linelen);
	}
	free(line);
	fclose(fp);
}

/*** APPEND BUFFER ***/

struct abuff {
	char *buffer;
	int len;
};

void bufferAppend(struct abuff *ab, const char *tobeadded, int len)
{
	char *new_buffer = realloc(ab->buffer, ab->len + len);
	if (new_buffer == NULL)
		return;
	
	memcpy(&new_buffer[ab->len], tobeadded, len);
	ab->buffer = new_buffer;
	ab->len += len;
}

void bufferFree(struct abuff *ab)
{
	free(ab->buffer);
}

/*** OUTPUT ***/

void editorScroll() 
{
	if (E.cy < E.rowoff) 
		E.rowoff = E.cy;
	if (E.cy >= E.rowoff + E.screenrows)
		E.rowoff = E.cy - E.screenrows + 1;
}

void editorDrawRows(struct abuff *ab)
{
	for (int y = 0; y < E.screenrows; y++) {
		int filerow = y + E.rowoff;
		if (filerow >= E.numrows) {
			if (E.numrows == 0 && y == E.screenrows / 3) {
				char welcomebuff[80];
				int welcomelen = snprintf(welcomebuff, sizeof(welcomebuff), GREEN "Kaze (風) - Text editor" RESET);
				if (welcomelen > E.screencols) 
					welcomelen = E.screenrows;

				int padding = (E.screencols + 10 - welcomelen) / 2; // the +10 is for the 'GREEN' and 'RESET' sequences
				if (padding) {
					bufferAppend(ab, "~", 1);
					padding--;
				}
				while (padding--) 
					bufferAppend(ab, " ", 1);

				bufferAppend(ab, welcomebuff, welcomelen);
			} else {
				bufferAppend(ab, "~", 1);
			}
		} else {
			int len = E.row[filerow].size;
			if (len > E.screencols) 
				len = E.screencols;
			bufferAppend(ab, E.row[filerow].chars, len);
		}
		bufferAppend(ab, CLEAR_LINE_RIGHT, 3); // clear line to the right
		if (y != E.screenrows - 1) {
			bufferAppend(ab, "\r\n", 2);
		}
	}
}

void editorRefreshScreen()
{
	editorScroll();

	struct abuff ab = ABUFF_INIT; 

	bufferAppend(&ab, CURSOR_HIDE, 6);
	// bufferAppend(&ab, CLEAR_SCREEN,4); -- replaced with CLEAR_LINE in drawRows
	bufferAppend(&ab, CURSOR_HOME, 3);
	
	editorDrawRows(&ab);
	
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, E.cx + 1);
	bufferAppend(&ab, buf, strlen(buf));
	
	bufferAppend(&ab, CURSOR_SHOW, 6);

	write(STDOUT_FILENO, ab.buffer, ab.len);
	bufferFree(&ab);
}

/*** INPUT ***/

void editorMoveCursor(int key)
{	
	switch (key) {
		case ARROW_UP:
			if (E.cy != 0)
				E.cy--;
			break;
		case ARROW_DOWN:
			if (E.cy < E.numrows)
				E.cy++;
			break;
		case ARROW_LEFT:
			if (E.cx != 0)
				E.cx--;
			break;
		case ARROW_RIGHT:
			if (E.cx != E.screencols - 1)
				E.cx++;
			break;
	}
}

void editorMapKeypress()
{
	int key = editorReadKey();

	switch (key) {
		case CTRL_KEY('q'): {
			write(STDOUT_FILENO, CLEAR_SCREEN, 4);
			write(STDOUT_FILENO, CURSOR_HOME, 3);
			exit(0);
			break;
		}

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(key);
			break;

		case PAGE_UP:
		case PAGE_DOWN: {
			int times = E.screenrows;
			while (times--)
				editorMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			break;
		}

		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			E.cx = E.screencols - 1;
			break;
	}
}

/*** INIT ***/

void initEditor()
{
	E.cx = 0; E.cy = 0;
	E.rowoff = 0;
	E.row = NULL;
	E.numrows = 0;
	
	if (getWinSize(&E.screenrows, &E.screencols) == -1)
		die("getWinSize");
}

int main(int argc, char *argv[]) 
{
	enterRawMode();
	initEditor();
	if (argc >= 2) {
		editorOpen(argv[1]);
	} else {
		printf(YELLOW "usage: ./kaze <filename>\r\n" RESET);
		return 0;
	}

	while (1) {
		editorRefreshScreen();
		editorMapKeypress();
	}

	return 0;
}