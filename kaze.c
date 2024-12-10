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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
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
#define INVERT				ESC_("[7m")					// 4

#define CTRL_KEY(k) 		((k) & (0x1F))
#define ABUFF_INIT 			{NULL, 0}
#define KAZE_VERSION		"0.0.1"
#define KAZE_TAB_STOP		4

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
	int rsize;
	char *render;
} erow;

struct editorConfiguration {
	struct termios org_term;
	erow *row;
	char *filename;
	char statusmsg[80];
	time_t statusmsg_time;
	int numrows;
	int screenrows;
	int screencols;
	int rowoff;
	int coloff;
	int cx, cy;	// x - horizontal (cols), y - vertical(rows) 
	int rx;
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

	if (key == ESC) { // Check if key press == ESC
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

int editorRowCxToRx(erow *row, int cx)
{
	int rx = 0;
	for (int i = 0; i < cx; i++) {
		if (row->chars[i] == '\t')
			rx += (KAZE_TAB_STOP - 1) - (rx % KAZE_TAB_STOP);
		rx++;
	}
	return rx;
}

void editorUpdateRow(erow *row)
{
	int tabs = 0;
	for (int i = 0; i < row->size; i++)
		if (row->chars[i] == '\t')
			tabs++;

	free(row->render);
	row->render = malloc(row->size + tabs * (KAZE_TAB_STOP - 1) + 1);

	int idx = 0;
	for (int i = 0; i < row->size; i++) {
		if (row->chars[i] == '\t') {
			row->render[idx++] = ' ';
			while (idx % KAZE_TAB_STOP != 0)
				row->render[idx++] = ' ';
		} else {
			row->render[idx++] = row->chars[i];
		}
	}

	row->render[idx] = '\0';
	row->rsize = idx;
}

void editorAppendRow(char *line, size_t linelen)
{
	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

	int idx = E.numrows;
	E.row[idx].size = linelen;
	E.row[idx].chars = malloc(linelen + 1);
	memcpy(E.row[idx].chars, line, linelen);
	E.row[idx].chars[linelen] = '\0';

	E.row[idx].rsize = 0;
	E.row[idx].render = NULL;
	editorUpdateRow(&E.row[idx]);

	E.numrows++;
}

/*** FILE I/O ***/

void editorOpen(char *filename)
{
	free(E.filename);
	E.filename = strdup(filename);

	FILE *fp = fopen(filename, "r");
	if (!fp)
		die("fopen");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
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
	E.rx = 0;

	if (E.cy < E.numrows) 
		E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
	if (E.cy >= E.rowoff + E.screenrows)
		E.rowoff = E.cy - E.screenrows + 1;

	if (E.rx < E.coloff)
		E.coloff = E.rx;
	if (E.rx >= E.coloff + E.screencols)
		E.coloff = E.rx - E.screencols + 1;
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
			int len = E.row[filerow].rsize - E.coloff;
			if (len < 0) 
				len = 0;
			if (len > E.screencols) 
				len = E.screencols;
			bufferAppend(ab, &E.row[filerow].render[E.coloff], len);
		}

		bufferAppend(ab, CLEAR_LINE_RIGHT, 3); // clear line to the right
		bufferAppend(ab, "\r\n", 2);
	}
}

void editorDrawStatBar(struct abuff *ab)
{
	bufferAppend(ab, INVERT, 4);
	char status[80], rstatus[80];
	int len = snprintf(status, sizeof(status), "%.20s - %d lines", (E.filename) ? E.filename : "[No Name]", E.numrows);
	int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);
	if (len > E.screencols)
		len = E.screencols;
	bufferAppend(ab, status, len);
	while (len < E.screencols) {
		if (E.screencols - len == rlen) {
			bufferAppend(ab, rstatus, rlen);
			break;
		} else {
			bufferAppend(ab, " ", 1);
			len++;
		}
	}
	bufferAppend(ab, RESET, 4);
	bufferAppend(ab, "\r\n", 2);
}

void editorDrawMsgBar(struct abuff *ab)
{
	bufferAppend(ab, CLEAR_LINE_RIGHT, 3);
	int msglen = strlen(E.statusmsg);
	if (msglen > E.screencols)
		msglen = E.screencols;
	if (msglen && time(NULL) - E.statusmsg_time < 5)
		bufferAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen()
{
	editorScroll();

	struct abuff ab = ABUFF_INIT; 

	bufferAppend(&ab, CURSOR_HIDE, 6);
	// bufferAppend(&ab, CLEAR_SCREEN,4); -- replaced with CLEAR_LINE in drawRows
	bufferAppend(&ab, CURSOR_HOME, 3);
	
	editorDrawRows(&ab);
	editorDrawStatBar(&ab);
	editorDrawMsgBar(&ab);
	
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
	bufferAppend(&ab, buf, strlen(buf));
	
	bufferAppend(&ab, CURSOR_SHOW, 6);

	write(STDOUT_FILENO, ab.buffer, ab.len);
	bufferFree(&ab);
}

void editorSetStatMessage(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
	va_end(ap);
	E.statusmsg_time = time(NULL);
}

/*** INPUT ***/

void editorMoveCursor(int key)
{	
	erow *tmprow = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

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
			if (E.cx != 0) {
				E.cx--;
			} else if (E.cy > 0) {
				E.cy--;
				E.cx = E.row[E.cy].size;
			}
			break;
		case ARROW_RIGHT:
			if (tmprow && E.cx < tmprow->size) {
				E.cx++;
			} else if (tmprow && E.cx == tmprow->size) {
				E.cy++;
				E.cx = 0;
			}
			break;
	}

	tmprow = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	int rowlen = tmprow ? tmprow->size : 0;
	if (E.cx > rowlen)
		E.cx = rowlen;
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
			if (key == PAGE_UP) {
				E.cy = E.rowoff;
			} else if (key == PAGE_DOWN) {
				E.cy = E.rowoff + E.screenrows - 1;
				if (E.cy > E.numrows)
					E.cy = E.numrows;
			}

			int times = E.screenrows;
			while (times--)
				editorMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			break;
		}

		case HOME_KEY:
			if (E.cy < E.numrows)
				E.cx = E.row[E.cy].size;
			break;
		case END_KEY:
			if (E.cy < E.numrows)
				E.cx = E.row[E.cy].size;
			break;
	}
}

/*** INIT ***/

void initEditor()
{
	E.cx = 0; 
	E.cy = 0;
	E.rx = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.row = NULL;
	E.numrows = 0;
	E.filename = NULL;
	E.statusmsg[0] = '\0';
	E.statusmsg_time = 0;
	
	if (getWinSize(&E.screenrows, &E.screencols) == -1)
		die("getWinSize");
	E.screenrows -= 2;
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

	editorSetStatMessage("HELP: Ctrl-Q = quit");

	while (1) {
		editorRefreshScreen();
		editorMapKeypress();
	}

	return 0;
}