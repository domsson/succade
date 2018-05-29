#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <spawn.h>
#include <wordexp.h>
#include "ini.h"

#define DEBUG 0 
#define NAME "succade"
#define BLOCKS_DIR "blocks"
#define BAR_PROCESS "lemonbar"

extern char **environ;      // Required to pass the env to child cmds

struct bar
{
	char *name;             // Name of the bar/process
	FILE *fd_in;            // File descriptor for writing to bar
	FILE *fd_out;           // File descriptor for reading from bar
	char *fg;               // Foreground color
	char *bg;               // Background color
	char *lc;               // Overline/underline color
	char *prefix;           // Prepend this to every block's result
	char *suffix;           // Append this to every block's result
	size_t line_width : 8;  // Overline/underline width in px
	size_t width : 16;
	size_t height : 16;
	size_t x : 16;
	size_t y : 16;
	int bottom : 1;         // Position bar at bottom of screen?
	int force : 1;          // Force docking?
	char *format;           // List and position of blocks
};

struct block
{
	char *name;             // Name of the block and its file/process
	char *path;             // Full path, including file name
	FILE *fd;               // File descriptor as returned by popen()
	char *fg;               // Foreground color
	char *bg;               // Background color
	char *lc;               // Overline/underline color
	int ol : 1;             // Draw overline?
	int ul : 1;             // Draw underline?
	size_t padding : 8;     // Minimum width of result in chars
	size_t offset : 16;     // Offset to next block in px
	int align;              // -1, 0, 1 (left, center, right)
	char *label;            // Prefixes the result string
	char *trigger;          // Run block based on this cmd
	char *m_left;           // Command to run on left mouse click
	char *m_middle;         // Command to run on middle mouse click
	char *m_right;          // Command to run on right mouse click
	char *s_up;             // Command to run on scroll up
	char *s_down;           // Command to run on scroll down
	int used : 1;           // Has this block been run at least once?
	double reload;          // Interval between runs 
	double waited;          // Time the block hasn't been run
	char *input;            // Recent output of the associated trigger
	char *result;           // Output of the most recent block run
};

struct trigger
{
	char *cmd;              // Command to run
	FILE *fd;               // File descriptor as returned by popen()
	struct block *b;        // Associated block
	struct bar *bar;	// Associated bar (special use case...)
	int ready : 1;          // fd has new data available for reading
};

/*
 * Init the given bar struct to a well defined state using sensible defaults.
 */
void init_bar(struct bar *b)
{
	b->name = NULL;
	b->fd_in = NULL;
	b->fd_out = NULL;
	b->fg = NULL;
	b->bg = NULL;
	b->lc = NULL;
	b->line_width = 1;
	b->width = 0;
	b->height = 0;
	b->x = 0;
	b->y = 0;
	b->bottom = 0;
	b->force = 0;
	b->prefix = NULL;
	b->suffix = NULL;
	b->format = NULL;
}

/*
 * Init the given block struct to a well defined state using sensible defaults.
 */
void init_block(struct block *b)
{
	b->name = NULL;
	b->path = NULL;
	b->fd = NULL;
	b->fg = NULL;
	b->bg = NULL;
	b->lc = NULL;
	b->ul = 0;
	b->ol = 0;
	b->padding = 0;
	b->offset = 0;
	b->align = 0;
	b->label = NULL;
	b->used = 0;
	b->trigger = NULL;
	b->m_left = NULL;
	b->m_middle = NULL;
	b->m_right = NULL;
	b->s_up = NULL;
	b->s_down = NULL;
	b->reload = 5.0;
	b->waited = 0.0;
	b->input = NULL;
	b->result = NULL;
}

void free_bar(struct bar *b)
{
	free(b->fg);
	free(b->bg);
	free(b->lc);
	free(b->prefix);
	free(b->suffix);
	free(b->format);

	init_bar(b); // Sets all pointers to NULL, just in case
}

void free_block(struct block *b)
{
	free(b->name);
	free(b->path);
	free(b->fg);
	free(b->bg);
	free(b->lc);
	free(b->label);
	free(b->trigger);
	free(b->m_left);
	free(b->m_middle);
	free(b->m_right);
	free(b->s_up);
	free(b->s_down);
	free(b->input);
	free(b->result);

	init_block(b); // Sets all pointers to NULL, just in case
}

/*
 * Returns 1 if both input strings are equal, otherwise 0.
 */
int equals(const char *str1, const char *str2)
{
	return strcmp(str1, str2) == 0;
}

/*
 * Returns 1 if the input string is quoted, otherwise 0.
 */
int is_quoted(const char *str)
{
	size_t len = strlen(str); // Length without null terminator
	if (len < 2) return 0;    // We need at least two quotes (empty string)
	char first = str[0];
	char last  = str[len - 1];
	if (first == '\'' && last == '\'') return 1; // Single-quoted string
	if (first == '"' && last == '"') return 1;   // Double-quoted string
	return 0;
}

/*
 * Returns a pointer to a string that is the same as the input string, 
 * minus the enclosing quotation chars (either single or double quotes).
 * The pointer is allocated with malloc(), the caller needs to free it.
 */
char *unquote(const char *str)
{
	char *trimmed = NULL;
	size_t len = strlen(str);
	if (len < 2) // Prevent zero-length allocation
	{
		trimmed = malloc(1); // Make space for null terminator
		trimmed[0] = '\0';   // Add the null terminator
	}
	else
	{
		trimmed = malloc(len-2+1);        // No quotes, null terminator
		strncpy(trimmed, &str[1], len-2); // Copy everything in between
		trimmed[len-2] = '\0';            // Add the null terminator
	}
	return trimmed;
}

pid_t popen2(const char *cmd, FILE *pipes[2])
{
	if (!cmd || !strlen(cmd))
	{
		return 0;
	}

	int pipe_write[2];
	int pipe_read[2];

	// [0] = read end, [1] = write end
	if (pipe(pipe_write) < 0) printf("can't open stdin pipe\n");
	if (pipe(pipe_read) < 0) printf("can't open stdout pipe\n");

	pid_t pid = fork();
	if (pid == -1)
	{
		printf("can't fork\n");
	}
	else if (pid == 0) // child
	{
		if (dup2(pipe_write[0], STDIN_FILENO) == -1) printf("dup2 stdin failed\n");
		if (dup2(pipe_read[1], STDOUT_FILENO) == -1) printf("dup2 stdout failed\n");
		if (dup2(pipe_read[1], STDERR_FILENO) == -1) printf("dup2 stderr failed\n");

		close(pipe_write[1]);
		close(pipe_read[0]);

		wordexp_t p;
		wordexp(cmd, &p, 0);
		
		execvp(p.we_wordv[0], p.we_wordv);
		_exit(1);
	}
	else // parent
	{
		close(pipe_write[0]);
		close(pipe_read[1]);
		pipes[0] = fdopen(pipe_read[0], "r");
		pipes[1] = fdopen(pipe_write[1], "w");
		return pid;
	}
}

int open_bar(struct bar *b)
{
	char width[8];
	char height[8];

	snprintf(width, 8, "%d", b->width);
	snprintf(height, 8, "%d", b->height);

	char barprocess[512];
	snprintf(barprocess, 512,
		"%s -g %sx%s+%d+%d -F%s -B%s -U%s -u%d %s %s",
		BAR_PROCESS,
		(b->width > 0) ? width : "",
		(b->height > 0) ? height : "",
		b->x,
		b->y,
		(b->fg && strlen(b->fg)) ? b->fg : "-", 
		(b->bg && strlen(b->bg)) ? b->bg : "-",
		(b->lc && strlen(b->lc)) ? b->lc : "-",	
		b->line_width,
		(b->bottom) ? "-b" : "",
		(b->force)  ? "-f" : ""
	);

	printf("Bar process:\n\t%s\n", barprocess);	

	/*
	b->fd = popen(barprocess, "w"); // Open in write mode
	if (b->fd == NULL)
	{
		return 0;
	}
	setlinebuf(b->fd);	// Make sure the stream is line buffered
	*/

	FILE *fd[2];
	int pid = popen2(barprocess, fd);
	setlinebuf(fd[0]);
	setlinebuf(fd[1]);
	b->fd_in = fd[1];
	b->fd_out = fd[0];

	return 1;
}

int open_block(struct block *b)
{
	if (b->input)
	{
		size_t cmd_len = strlen(b->path) + strlen(b->input) + 4;
		char *cmd = malloc(cmd_len);
		snprintf(cmd, cmd_len, "%s '%s'", b->path, b->input);
		b->fd = popen(cmd, "r");
		free(cmd);
	}
	else
	{
		b->fd = popen(b->path, "r");
	}
	return (b->fd == NULL) ? 0 : 1;
}

/*
 * Convenience function: simply runs open_block() for all blocks.
 */
void open_blocks(struct block *blocks, int num_blocks)
{
	for(int i=0; i<num_blocks; ++i)
	{
		open_block(&blocks[i]);
	}
}

void close_bar(struct bar *b)
{
	if (b->fd_in != NULL)
	{
		fclose(b->fd_in);
	}
	if (b->fd_out != NULL)
	{
		fclose(b->fd_out);
	}
}

int close_block(struct block *b)
{
	if (b->fd == NULL)
	{
		return 0;
	}
	pclose(b->fd);
	b->fd = NULL;	
	return 1;
}

void close_blocks(struct block *blocks, int num_blocks)
{
	for(int i=0; i<num_blocks; ++i)
	{
		close_block(&blocks[i]);
	}
}

int open_trigger(struct trigger *t)
{
	if (t->cmd == NULL)
	{
		return 0;
	}
	t->fd = popen(t->cmd, "r");
	if (t->fd == NULL)
	{
		printf("Failed to open trigger: %s\n", t->cmd);
		return 0;
	}
	setlinebuf(t->fd);
	int fn = fileno(t->fd);
	int flags;
	flags = fcntl(fn, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(fn, F_SETFL, flags);
	return 1;
}

int close_trigger(struct trigger *t)
{
	if (t->fd == NULL)
	{
		return 0;
	}
	pclose(t->fd);
	t->fd = NULL;
	return 1;
}

int open_triggers(struct trigger *triggers, int num_triggers)
{
	int num_triggers_opened = 0;
	for (int i=0; i<num_triggers; ++i)
	{
		num_triggers_opened += open_trigger(&triggers[i]);
	}
	return num_triggers_opened;
}

void close_triggers(struct trigger *triggers, int num_triggers)
{
	for (int i=0; i<num_triggers; ++i)
	{
		close_trigger(&triggers[i]);
	}
}

int free_trigger(struct trigger *t)
{
	free(t->cmd);
	t->cmd = NULL;

	t->b = NULL;
	t->bar = NULL;
}

void free_blocks(struct block *blocks, int num_blocks)
{
	for (int i=0; i<num_blocks; ++i)
	{
		free_block(&blocks[i]);
	}
}

void free_triggers(struct trigger *triggers, int num_triggers)
{
	for (int i=0; i<num_triggers; ++i)
	{
		free_trigger(&triggers[i]);
	}
}

int run_block(struct block *b, size_t result_length)
{
	open_block(b);
	if (b->fd == NULL)
	{
		printf("Block is dead: `%s`", b->name);
		return 0;
	}
	if (b->result != NULL)
	{
		free(b->result);
		b->result = NULL;
	}
	b->result = malloc(result_length);
	if (fgets(b->result, result_length, b->fd) == NULL)
	{
		printf("Unable to fetch input from block: `%s`", b->name);
		close_block(b);
		return 0;
	}
	b->result[strcspn(b->result, "\n")] = 0; // Remove '\n'
	b->used = 1; // Mark this block as having run at least once
	b->waited = 0.0; // This block was last run... now!
	close_block(b);
	return 1;
}

char *escape(const char *str, size_t *diff)
{
	int n = 0; // number of % chars
	char c = 0; // current char
	int i = 0;
	while ((c = str[i]) != '\0')
	{
		if (c == '%')
		{
			++n;
		}
		++i;
	}
	if (diff)
	{
		*diff = n;
	}

	char *escstr = malloc(i+n+1);
	int k=0;
	for (int j=0; j<i; ++j)
	{
		if (str[j] == '%')
		{
			escstr[k++] = '%';
			escstr[k++] = '%';
		}
		else
		{
			escstr[k++] = str[j];
		}
	}
	escstr[k] = '\0';
	return escstr;
}

/*
 * Given a block, it returns a pointer to a string that is the formatted result 
 * of this block's script output, ready to be fed to Lemonbar, including prefix,
 * label and suffix. The string is malloc'd and should be free'd by the caller.
 */
char *blockstr(const struct bar *bar, const struct block *block, size_t len)
{
	char action_start[512];
	action_start[0] = 0;
	char action_end[256];
	action_end[0] = 0;

	if (block->m_left)
	{
		strcat(action_start, "%{A:");
		strcat(action_start, block->name);
		strcat(action_start, "_lmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->m_middle)
	{
		strcat(action_start, "%{A:");
		strcat(action_start, block->name);
		strcat(action_start, "_mmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->m_right)
	{
		strcat(action_start, "%{A:");
		strcat(action_start, block->name);
		strcat(action_start, "_rmb:}");
		strcat(action_end, "%{A}");
	}

	size_t diff;
	char *result = escape(block->result, &diff);

	char *str = malloc(len + diff);
	snprintf(str, len,
		"%s%%{O%d}%%{F%s}%%{B%s}%%{U%s}%%{%co%cu}%s%s%*s%s%%{F-}%%{B-}%%{U-}%s",
		action_start,
		block->offset,
		block->fg && strlen(block->fg) ? block->fg : "-",
		block->bg && strlen(block->bg) ? block->bg : "-",
		block->lc && strlen(block->lc) ? block->lc : "-",
		block->ol ? '+' : '-',
		block->ul ? '+' : '-',
		bar->prefix ? bar->prefix : "",
		block->label ? block->label : "",
		block->padding + diff,
		result,
		bar->suffix ? bar->suffix : "",
		action_end
	);
	free(result);
	return str;
}

char get_align(const int align)
{
	if (align == -1)
	{
		return 'l';
	}
	if (align == 0)
	{
		return 'c';
	}
	if (align == 1)
	{
		return 'r';
	}
}

/*
 * Combines the results of all given blocks into a single string that can be fed
 * to Lemonbar. Returns a pointer to the string, allocated with malloc().
 */
char *barstr(const struct bar *bar, const struct block *blocks, size_t num_blocks)
{
	size_t blockstr_len = 256;
	char *bar_str = malloc(blockstr_len * num_blocks);
	bar_str[0] = '\0';

	char align[5];
	int last_align = blocks[0].align;
	snprintf(align, 5, "%%{%c}", get_align(last_align));
	strcat(bar_str, align);

	for (int i=0; i<num_blocks; ++i)
	{
		char *block_str = blockstr(bar, &blocks[i], blockstr_len);
		if (blocks[i].align != last_align)
		{
			last_align = blocks[i].align;
			snprintf(align, 5, "%%{%c}", get_align(last_align));
			strcat(bar_str, align);
		}	
		strcat(bar_str, block_str);
		free(block_str);
	}
	strcat(bar_str, "\n");
	bar_str = realloc(bar_str, strlen(bar_str) + 1);
	return bar_str;
}

int feed_bar(struct bar *bar, struct block *blocks, size_t num_blocks, double delta, double *next)
{
	if (bar->fd_in == NULL)
	{
		perror("Bar seems dead");
		return 0;
	}

	int num_blocks_executed = 0;	
	double until_next = 5;

	for(int i=0; i<num_blocks; ++i)
	{
		blocks[i].waited += delta;
		if (!blocks[i].used || blocks[i].input || blocks[i].waited >= blocks[i].reload)
		{
			num_blocks_executed += run_block(&blocks[i], 64);
		}
		if (blocks[i].input == NULL && (blocks[i].reload - blocks[i].waited) < until_next)
		{
			until_next = blocks[i].reload - blocks[i].waited;
		}
	}
	*next = until_next;

	if (num_blocks_executed)
	{
		char *lemonbar_str = barstr(bar, blocks, num_blocks);
		if (DEBUG) { printf("%s", lemonbar_str); }
		//fputs(lemonbar_str, bar->fd);
		fputs(lemonbar_str, bar->fd_in);
		free(lemonbar_str);
	}
	return num_blocks_executed;
}

/*
 * Concatenates the given directory, file name and file extension strings
 * to a complete path. The fileext argument is optional, it can be set to NULL.
 * Returns a pointer to the concatenated string, allocated via malloc().
 */
char *filepath(const char *dir, const char *filename, const char *fileext)
{
	if (fileext != NULL)
	{
		size_t path_len = strlen(dir) + strlen(filename) + strlen(fileext) + 3;
		char *path = malloc(path_len);
		snprintf(path, path_len, "%s/%s.%s", dir, filename, fileext);
		return path;
	}
	else
	{
		size_t path_len = strlen(dir) + strlen(filename) + 2;
		char *path = malloc(path_len);
		snprintf(path, path_len, "%s/%s", dir, filename);
		return path;
	}
}

/*
 * Parse the given format string and create blocks accordingly.
 * Those blocks only have their name, path and align properties set.
 * Returns the number of blocks created.
 */
int parse_format(const char *format, struct block **blocks, const char *blockdir)
{
	size_t size = 8;
	*blocks = malloc(size * sizeof(struct block));
	size_t format_len = strlen(format) + 1;
	char cur_block_name[64];
	     cur_block_name[0] = '\0';
	size_t cur_block_len = 0;
	int cur_align = -1;
	int num_blocks = 0;
	for(int i=0; i<format_len; ++i)
	{
		switch (format[i])
		{
		case '|':
			if (cur_align < 1)
			{
				++cur_align;
			}
		case ' ':
		case '\0':
			if (cur_block_len)
			{
				if (num_blocks == size)
				{
					size += 2;
					*blocks = realloc(*blocks, size * sizeof(struct block));
				}
				struct block b;
				init_block(&b);
				b.name = strdup(cur_block_name);
				b.path = filepath(blockdir, cur_block_name, NULL);
				b.align = cur_align;
				(*blocks)[num_blocks++] = b;
				cur_block_name[0] = '\0';
				cur_block_len = 0;
			}
			break;
		default:
			cur_block_name[cur_block_len++] = format[i];
			cur_block_name[cur_block_len]   = '\0';
		}
	}
	*blocks = realloc(*blocks, sizeof(struct block) * num_blocks);
	return num_blocks;
}

static int bar_ini_handler(void *b, const char *section, const char *name, const char *value)
{
	struct bar *bar = (struct bar*) b;
	if (equals(name, "name"))
	{
		bar->name = strdup(value);
		return 1;
	}
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		bar->fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		bar->bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "line"))
	{
		bar->lc = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "line-width"))
	{
		bar->line_width = atoi(value);
		return 1;
	}
	if (equals(name, "h") || equals(name, "height"))
	{
		bar->height = atoi(value);
		return 1;
	}
	if (equals(name, "w") || equals(name, "width"))
	{
		bar->width = atoi(value);
		return 1;
	}
	if (equals(name, "x"))
	{
		bar->x = atoi(value);
		return 1;
	}
	if (equals(name, "y"))
	{
		bar->y = atoi(value);
		return 1;
	}
	if (equals(name, "dock"))
	{
		bar->bottom = (equals(value, "bottom")) ? 1 : 0;
		return 1;
	}
	if (equals(name, "force"))
	{
		bar->force = (equals(value, "true")) ? 1 : 0;
		return 1;
	}
	if (equals(name, "prefix") || equals(name, "block-prefix"))
	{
		bar->prefix = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "suffix") || equals(name, "block-suffix"))
	{
		bar->suffix = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "format"))
	{
		bar->format = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	return 0; // unknown section/name, error
}

int configure_bar(struct bar *b, const char *config_dir)
{
	char rc[256];
	snprintf(rc, sizeof(rc), "%s/%src", config_dir, NAME);
	if (ini_parse(rc, bar_ini_handler, b) < 0)
	{
		printf("Can't parse rc file %s\n", rc);
		return 0;
	}
	return 1;
}

static int block_ini_handler(void *b, const char *section, const char *name, const char *value)
{
	struct block *block = (struct block*) b;
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		block->fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		block->bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "lc") || equals(name, "line"))
	{
		block->lc = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "ol") || equals(name, "overline"))
	{
		block->ol = equals(value, "true") ? 1 : 0;
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		block->ul = equals(value, "true") ? 1 : 0;
		return 1;
	}
	if (equals(name, "pad") || equals(name, "padding"))
	{
		block->padding = atoi(value);
		return 1;
	}
	if (equals(name, "offset"))
	{
		block->offset = atoi(value);
		return 1;
	}
	if (equals(name, "label"))
	{
		block->label = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "reload"))
	{
		if (is_quoted(value)) // String means trigger!
		{
			block->trigger = unquote(value);
		}
		else
		{
			block->reload = atof(value);
		}
		return 1;
	}
	if (equals(name, "mouse-left"))
	{
		block->m_left = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "mouse-middle"))
	{
		block->m_middle = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "mouse-right"))
	{
		block->m_right = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "scroll-up"))
	{
		block->s_up = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "scroll-down"))
	{
		block->s_down = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	return 0; // unknown section/name or error
}

/*
 * Load the configuration file (ini) for the given block, if it exists,
 * and have the ini reader (inih) call block_ini_handler() accordingly.
 * Returns 1 on success, 0 if the config file does'n exist or couldn't be read.
 */
int configure_block(struct block *b, const char *blocks_dir)
{
	char *blockini = filepath(blocks_dir, b->name, "ini");
	if (access(blockini, R_OK) == -1)
	{
		//printf("No block config found for: %s\n", b->name);
		return 0;
	}
	if (ini_parse(blockini, block_ini_handler, b) < 0)
	{
		printf("Can't parse block INI: %s\n", blockini);
		free(blockini);
		return 0;
	}
	free(blockini);
	return 1;
}

/*
 * Convenience function: simply runs configure_block() for all blocks.
 */
int configure_blocks(struct block *blocks, int num_blocks, const char *blocks_dir)
{
	for (int i=0; i<num_blocks; ++i)
	{
		configure_block(&blocks[i], blocks_dir);
	}
}

/*
 * Returns 1 if the given file name ends in ".ini", 0 otherwise.
 */
int is_ini(const char *filename)
{
	char *dot = strrchr(filename, '.');
	return (dot && !strcmp(dot, ".ini")) ? 1 : 0;
}

/*
 * Returns 1 if the given file name starts with ".", 0 otherwise.
 */
int is_hidden(const char *filename)
{
	return filename[0] == '.';
}

int is_executable(const char *filename)
{
	return 1;
	// TODO this needs the PATH of the file as well...
	struct stat sb;
	return (stat(filename, &sb) == 0 && sb.st_mode & S_IXUSR);
}

int probably_a_block(const char *filename)
{
	return !is_ini(filename) && !is_hidden(filename) && is_executable(filename);
}

int create_triggers(struct trigger **triggers, struct block *blocks, int num_blocks)
{
	if (num_blocks == 0)
	{
		*triggers = NULL;
		return 0;
	}
	*triggers = malloc(num_blocks * sizeof(struct trigger));
	int num_triggers_created = 0;
	for (int i=0; i<num_blocks; ++i)
	{
		if (blocks[i].trigger == NULL)
		{
			continue;
		}
		struct trigger t = {
			.cmd = strdup(blocks[i].trigger),
			.fd = NULL,
			.b = &blocks[i],
			.bar = NULL,
			.ready = 0
		};
		(*triggers)[num_triggers_created++] = t;
	}
	*triggers = realloc(*triggers, num_triggers_created * sizeof(struct trigger));
	return num_triggers_created;
}

int count_blocks(const char *blockdir)
{
	DIR *block_dir = opendir(blockdir);
	if (block_dir == NULL)
	{
		return -1;
	}

	int num_blocks = 0;
	struct dirent *entry;
	while ((entry = readdir(block_dir)) != NULL)
	{
		if (entry->d_type == DT_REG && probably_a_block(entry->d_name))
		{
			++num_blocks;
		}
	}
	return num_blocks;
}

int create_blocks(struct block **blocks, const struct block *blocks_req, int num_blocks_req)
{
	*blocks = malloc(num_blocks_req * sizeof(struct block));

	size_t num_blocks_created = 0;
	for (int i=0; i<num_blocks_req; ++i)
	{
		if (access(blocks_req[i].path, F_OK|R_OK|X_OK) != -1)
		{
			(*blocks)[num_blocks_created++] = blocks_req[i];
		}
	}
	*blocks = realloc(*blocks, num_blocks_created * sizeof(struct block));
	return num_blocks_created;
}

// NEW and UNTESTED
char *config_dir()
{
	char *home = getenv("HOME");
	char *cfg_home = getenv("XDF_CONFIG_HOME");
	char *cfg_dir = NULL;
	size_t cfg_dir_len;
	if (cfg_home == NULL)
	{
		cfg_dir_len = strlen(home) + strlen(".config") + strlen(NAME) + 3;
		cfg_dir = malloc(cfg_dir_len);
		snprintf(cfg_dir, cfg_dir_len, "%s/%s/%s", home, ".config", NAME);
	}
	else
	{
		cfg_dir_len = strlen(cfg_home) + strlen(NAME) + 2;
		cfg_dir = malloc(cfg_dir_len);
		snprintf(cfg_dir, cfg_dir_len, "%s/%s", cfg_home, NAME);
	}
	return cfg_dir;
}

int get_config_dir(char *buffer, int buffer_size)
{
	char *config_home = getenv("XDF_CONFIG_HOME");
	if (config_home != NULL)
	{
		return snprintf(buffer, buffer_size, "%s/%s",
			config_home,
			NAME
		);
	}
	else
	{
		return snprintf(buffer, buffer_size, "%s/%s/%s",
			getenv("HOME"),
			".config",
			NAME
		);
	}
}

int get_blocks_dir(char *buffer, int buffer_size)
{
	char *config_home = getenv("XDG_CONFIG_HOME");
	if (config_home != NULL)
	{
		return snprintf(buffer, buffer_size, "%s/%s/%s",
			config_home,
			NAME,
			BLOCKS_DIR
		);
	}
	else
	{
		return snprintf(buffer, buffer_size, "%s/%s/%s/%s",
			getenv("HOME"),
			".config",
			NAME,
			BLOCKS_DIR
		);
	}
}

double get_time()
{
	clockid_t cid = CLOCK_MONOTONIC;
	// TODO the next line is cool, as CLOCK_MONOTONIC is not
	// present on all systems, where CLOCK_REALTIME is, however
	// I don't want to call sysconf() with every single iteration
	// of the main loop, so let's do this ONCE and remember...
	//clockid_t cid = (sysconf(_SC_MONOTONIC_CLOCK) > 0) ? CLOCK_MONOTONIC : CLOCK_REALTIME;
	struct timespec ts;
	clock_gettime(cid, &ts);
	return (double) ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

/*
 * Read all pending lines from the given trigger and store only the last line 
 * in the corresponding block's input field. Previous lines will be discarded.
 * Returns the number of lines read from the trigger's file descriptor.
 */
int run_trigger(struct trigger *t)
{
	if (t->fd == NULL)
	{
		return 0;
	}
	char res[256];
	
	int num_lines = 0;
	while (fgets(res, 256, t->fd) != NULL)
	{
		++num_lines;
	}
	if (num_lines)
	{
		struct block *b = t->b;
		if (b->input != NULL)
		{
			free(b->input);
			b->input = NULL;
		}
		b->input = strdup(res);
	}
	return num_lines;
}

/*
 * Run the given command, which we want to do when the user triggers 
 * an action of a clickable area that has a command associated with it.
 */
pid_t run_cmd(const char *cmd)
{
	if (!cmd || !strlen(cmd))
	{
		return 0;
	}
	wordexp_t p;
	if (wordexp(cmd, &p, 0) != 0)
	{
		printf("Could not parse the command:\n\t'%s'\n", cmd);
		return 0;
	}
	
	/*
	for (int i=0; i<p.we_wordc; ++i)
	{
		printf("arg%d = %s\n",
			i,
			*(p.we_wordv + i)
		);
	}
	*/

	pid_t pid;
	int res = posix_spawnp(&pid, p.we_wordv[0], NULL, NULL, p.we_wordv, environ);
	wordfree(&p);

	return (res == 0 ? pid : 0);
}

void process_action(const char *action, struct block *blocks, int num_blocks)
{
	for (int i=0; i<num_blocks; ++i)
	{
	}
}

int main(void)
{
	// Prevent zombie children during runtime
	if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
	{
		printf("Failed to ignore children's signals\n");
	}
	
	char configdir[256];
	if (get_config_dir(configdir, sizeof(configdir)))
	{
		printf("Config directory:\n\t%s\n", configdir);
	}

	char blocksdir[256];
	if (get_blocks_dir(blocksdir, sizeof(blocksdir)))
	{
		printf("Blocks directory:\n\t%s\n", blocksdir);
	}

	/*
	 * BAR
	 */

	struct bar lemonbar;
	init_bar(&lemonbar);

	if (!configure_bar(&lemonbar, configdir))
	{
		printf("Failed to load RC file: %src\n", NAME);
		exit(1);
	}
	if (!open_bar(&lemonbar))
	{
		printf("Failed to open bar: %s\n", BAR_PROCESS);
		exit(1);
	}

	/*
	 * BLOCKS
	 */

	struct block *blocks_requested; 
	int num_blocks_requested = parse_format(lemonbar.format, &blocks_requested, blocksdir);

	printf("Blocks requested: (%d total)\n\t", num_blocks_requested);
	for (int i=0; i<num_blocks_requested; ++i)
	{
		printf("%s ", blocks_requested[i].name);
	}
	printf("\n");

	struct block *blocks;
	int num_blocks = create_blocks(&blocks, blocks_requested, num_blocks_requested);
	configure_blocks(blocks, num_blocks, blocksdir);
	free(blocks_requested);

	printf("Blocks found: (%d total)\n\t", num_blocks);
	for (int i=0; i<num_blocks; ++i)
	{
		printf("%s ", blocks[i].name);
	}
	printf("\n");

	/*
	 * BAR TRIGGER
	 */
	
	struct trigger bartrig = {
		.cmd = NULL,
		.fd = lemonbar.fd_out,
		.b = NULL,
		.bar = &lemonbar,
		.ready = 0
	};
	
	/*
	 * TRIGGERS
	 */

	struct trigger *triggers;
	int num_triggers = create_triggers(&triggers, blocks, num_blocks);

	printf("Triggers found: (%d total)\n\t", num_triggers);
	for (int i=0; i<num_triggers; ++i)
	{
		printf("'%s' ", triggers[i].cmd);
	}
	printf("\n");

	int num_triggers_opened = open_triggers(triggers, num_triggers);

	printf("Triggeres opened: (%d total)\n\t", num_triggers_opened);
	for (int i=0; i<num_triggers; ++i)
	{
		printf("'%s' ", triggers[i].cmd ? triggers[i].cmd : "");
	}
	printf("\n");

	/* 
	 * EVENTS
	 */

	int epfd = epoll_create(1);
	if (epfd < 0)
	{
		printf("Could not create epoll file descriptor\n");
		exit(1);
	}

	int epctl_result = 0;
	for (int i=0; i<num_triggers; ++i)
	{
		if (triggers[i].fd == NULL)
		{
			continue;
		}
		struct epoll_event eev = { 0 };
		eev.data.ptr = &triggers[i];
		eev.events = EPOLLIN | EPOLLET;
		epctl_result += epoll_ctl(epfd, EPOLL_CTL_ADD, fileno(triggers[i].fd), &eev);
	}

	struct epoll_event eev = { 0 };
	eev.data.ptr = &bartrig;
	eev.events = EPOLLIN | EPOLLET;
	epoll_ctl(epfd, EPOLL_CTL_ADD, fileno(bartrig.fd), &eev);

	if (epctl_result)
	{
		printf("%d trigger events could not be registered\n", -1 * epctl_result);
	}

	/*
	 * MAIN LOOP
	 */

	double now;
	double before = get_time();
	double delta;
	double wait;

	while (1)
	{
		now = get_time();
		delta = now - before;
		before = now;

		/*
		if (fgets(buf, 256, fdchild[0]) != NULL)
		{
			printf("OUTPUT: %s\n", buf);
		}
		*/

		struct epoll_event tev[num_triggers];
		int num_events = epoll_wait(epfd, tev, num_triggers, wait * 1000);

		// Mark triggers that fired as ready to be read
		for (int i=0; i<num_events; ++i)
		{
			if (tev[i].events & EPOLLIN)
			{
				((struct trigger*) tev[i].data.ptr)->ready = 1;
			}
		}	

		// Fetch input from all marked triggers
		for (int i=0; i<num_triggers; ++i)
		{
			if (triggers[i].ready)
			{
				run_trigger(&triggers[i]);
			}
		}

		// TODO process bartrig, if it is ready
		if (bartrig.ready)
		{
			char act[128];
			fgets(act, 128, lemonbar.fd_out);
			printf("action_registered: %s\n", act);
			process_action(act, blocks, num_blocks); 
			bartrig.ready = 0;
		}

		feed_bar(&lemonbar, blocks, num_blocks, delta, &wait);
	}

	close(epfd);
	free_blocks(blocks, num_blocks);
	free(blocks);
	close_triggers(triggers, num_triggers);
	free_triggers(triggers, num_triggers);
	free(triggers);
	close_bar(&lemonbar);
	free_bar(&lemonbar);
	return 0;
}
