#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include "main.h"
#include "render/render.h"
#include "editor/editor.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dirent.h>

struct ed_conf E;

char *commands[] = {"w", "q", "wq", "files", NULL};

void
die(const char* s)
{

    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[2H", 3);

    perror(s);
    exit(1);
}

void
abAppend(struct abuf *ab, char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void
abFree(struct abuf *ab)
{
    free(ab->b);
}

void
disable_raw()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.traw_backup) == -1)
	die("tcsetattr");
}

void
raw_mode()
{
    if (tcgetattr(STDIN_FILENO, &E.traw_backup) == -1) die("tcsetattr");
    atexit(disable_raw);

    struct termios raw = E.traw_backup;

    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP |ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag &= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

void
init_ed()
{
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.nrows = 0;
    E.filen = NULL;
    E.row = NULL;
    E.cmd_mode = 0;
    E.cmdbuf[0] = '\0';
    E.cmdlen = 0;
    
    if (get_winsize(&E.screenrows, &E.screencols) == -1) die("get_winsize");
    E.screenrows -= 2;
}

void 
openD(char *filename) 
{
    for (int i = 0; i < E.nrows; i++) ed_freerow(&E.row[i]);
    free(E.row);
    E.row = NULL;
    E.nrows = 0;
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;

    free(E.filen);
    E.filen = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
      while (linelen > 0 && (line[linelen - 1] == '\n' ||
                             line[linelen - 1] == '\r'))
        linelen--;
    ed_inrows(E.nrows, line, linelen);
    }
    free(line);
    fclose(fp);
}

void
saveD() 
{
    if (E.filen == NULL) return;
    
    int len;
    char *buf = ed_rto_str(&len);
    
    int fd = open(E.filen, O_RDWR | O_CREAT, 0644);
    if (fd != -1)
    {
	if (ftruncate(fd, len) != -1)
	{
	    if (write(fd, buf, len) == len)
	    {
		close(fd);
		free(buf);
		return;
	    }
	}
	close(fd);
    }
    free(buf);
}

void
fp_load()
{
    for (int i = 0; i < E.file_count; i++) free(E.file_list[i]);
    free(E.file_list);
    E.file_list = NULL;
    E.file_count = 0;
    E.file_list = realloc(E.file_list, sizeof(char*) * (E.file_count + 1));
    E.file_list[E.file_count++] = strdup("../");

    DIR *d = opendir(".");
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue; 
        E.file_list = realloc(E.file_list, sizeof(char*) * (E.file_count + 1));
        char name[300];
        if (entry->d_type == DT_DIR)
            snprintf(name, sizeof(name), "%s/", entry->d_name);
        else
            snprintf(name, sizeof(name), "%s", entry->d_name);
        E.file_list[E.file_count++] = strdup(name);
    }
    closedir(d);
    E.file_sel = 0;
}

void
process_key_press()
{
    int c = edit_read_key();
    erow *rotw = (E.cy >= E.nrows) ? NULL : &E.row[E.cy];

    if (E.cmd_mode) {
        if (c == '\r') {
	    char input_copy[256];
	    strncpy(input_copy, E.cmdbuf, sizeof(input_copy) -1);
	    input_copy[sizeof(input_copy) - 1] = '\0';
	    if (input_copy[0] != '\0')
		ed_parse_and_exec(input_copy);
	    else
		E.statusmsg[0] = '\0';
	    E.cmd_mode = 0;
            E.cmdbuf[0] = '\0';
            E.cmdlen = 0;
        } else if (c == '\x1b') {
            E.cmd_mode = 0;
            E.cmdbuf[0] = '\0';
            E.cmdlen = 0;
        } else if (c == BACKSPACE) {
            if (E.cmdlen > 0)
                E.cmdbuf[--E.cmdlen] = '\0';
	} else if (c == '\t') {
	    cmd_autoc();
	    if (E.cmd_sugg[0] != '\0') {
		strncpy(E.cmdbuf, E.cmd_sugg, sizeof(E.cmdbuf));
		E.cmdlen = strlen(E.cmdbuf);
		E.cmd_sugg[0] = '\0';
	    }
        } else if (E.cmdlen < (int)sizeof(E.cmdbuf) - 1) {
            E.cmdbuf[E.cmdlen++] = c;
            E.cmdbuf[E.cmdlen] = '\0';
        } 
        return; 
    }

    if (E.file_mode)
	{ 
	    if (c == '\r') {
	    for (int i = 0, drawn = 0; i < E.file_count; i++) {
		if (E.file_qlen > 0 && !strstr(E.file_list[i], E.file_query)) continue;
		if (drawn == E.file_sel)
		    { 
			char *name = E.file_list[i];
			int len = strlen(name); 
			if (name [len - 1] == '/'){
			    char dirname[256];
			    strncpy(dirname, name, len - 1);
			    dirname[len - 1] = '\0';
			    chdir(dirname);
			    fp_load();
			    E.file_mode = 1;
			    E.file_query[0] = '\0';
			    E.file_qlen = 0;
			    E.file_sel = 0;
			} else {
			    E.file_mode = 0;
			    openD(name);
			    ed_set_status("%s opened succesfully", name);
			}
		    break;
		    }
		drawn++;    
	    }
	} else if (c == '\x1b') {
	    E.file_mode = 0;
	} else if (c == AR_UP) {
	    if (E.file_sel > 0) E.file_sel--;
	} else if (c == AR_DOWN) {
	    int visible = 0;
	    for (int i = 0; i < E.file_count; i++) {
		if (E.file_qlen > 0 && !strstr(E.file_list[i], E.file_query)) continue;
		visible++;
	    }
	    if (E.file_sel < visible -1) E.file_sel++;
	} else if (c == BACKSPACE) {
	    if (E.file_qlen > 0) E.file_query[--E.file_qlen] = '\0';
	    E.file_sel = 0;
	} else if (c >= 32 && c < 127) {
	    if (E.file_qlen < (int)sizeof(E.file_query) - 1) {
		E.file_query[E.file_qlen++] = c;
	        E.file_query[E.file_qlen] = '\0';
	        E.file_sel = 0;
	    }
	}
	return;
    }
 
    switch (c)
    {
	case '\r':
	    in_nw();
	    break;
	case '\t':
	    ed_inch('\t');
	    break;
	case CTRL_KEY('q'):
	    write(STDOUT_FILENO, "\x1b[2J", 4);
	    write(STDOUT_FILENO, "\x1b[2H", 3);
	    exit(0);
	    break;
	case AR_UP:
	case AR_DOWN:
	case AR_LEFT:
	case AR_RIGHT:
	    ed_move_c(c);
	    break;
	case BACKSPACE:
	    ed_delch();
	    break;
	case CTRL_KEY('s'):
	    saveD();
	    ed_set_status("File writted");
	    break;
	case CTRL_KEY('f'):
	    fp_load();
	    E.file_mode = 1;
	    E.file_query[0] = '\0';
	    E.file_qlen = 0;
	    ed_set_status("Explorer: Type file name");
	    break;
	case CTRL_AR_RIGHT:
	    ed_move_word_right();
	    break;
	case CTRL_AR_LEFT:
	    ed_move_word_left();
	    break;
	case DEL_WORD_LEFT:
	    ed_del_word_left();
	    break;

	case '\x1b':
	    break;
	case CTRL_KEY('c'):
	    E.cmd_mode = 1;
	    E.cmdlen = 0;
	    E.cmdbuf[0] = '\0';
	    break;
	case CTRL_KEY('d'):
	    ed_del_row();
	    break;
	case CTRL_KEY('e'):
	    E.cx = rotw->size;
	    break;
	case CTRL_KEY('w'):
	    E.cx = 0;
	    break;
	case CTRL_KEY('j'):
	    ed_move_c(AR_DOWN);
	    break;
	case CTRL_KEY('k'):    
	    ed_move_c(AR_UP);
	    break;
	case CTRL_KEY('h'):
	    ed_move_c(AR_LEFT);
	    break;
	case CTRL_KEY('l'):
	    ed_move_c(AR_RIGHT);
	    break;

	default:
	    ed_inch(c);
	    if (c < 32 || c == 127) {
		char dbg[16];
		snprintf(dbg, sizeof(dbg), "<%d>", c);
		ed_set_status(dbg);
	    }
	    break;
    }
}

int
main(int argc, char *argv[])
{
    raw_mode();
    init_ed();

    if (argc >= 2)
	openD(argv[1]);

    if (argc >= 3)
    {
	int i;
	for (i = 0; i < E.screenrows; i++){
	    int filerow = i + E.nrows;
	    if (atoi(argv[2]) >= filerow)
		break;
	    E.cy = atoi(argv[2]);
	}
    }

    while (1){
	refresh_screen();
	process_key_press();
    }

    return 0;
}

