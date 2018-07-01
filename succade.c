#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <spawn.h>
#include <wordexp.h>
#include "ini.h"

#define DEBUG 0 
#define NAME "succade"
#define BLOCKS_DIR "blocks"
#define BAR_PROCESS "lemonbar"
#define BUFFER_SIZE 2048

extern char **environ;        // Required to pass the env to child cmds
static volatile int running;  // Used to stop main loop in case of SIGINT
static volatile int handled;  // The last signal that has been handled 

struct bar
{
	char *name;             // Name of the bar/process
	pid_t pid;		// Process ID of the bar 
	FILE *fd_in;            // File descriptor for writing to bar
	FILE *fd_out;           // File descriptor for reading from bar
	char *fg;               // Foreground color
	char *bg;               // Background color
	char *lc;               // Overline/underline color
	char *prefix;           // Prepend this to every block's result
	char *suffix;           // Append this to every block's result
	int ol : 1;		// Draw overline for all blocks?
	int ul : 1;		// Draw underline for all blocks?
	size_t lw : 8;          // Overline/underline width in px
	size_t w : 16;          // Width of the bar
	size_t h : 16;          // Height of the bar
	size_t x : 16;          // x-position of the bar
	size_t y : 16;          // y-position of the bar
	int bottom : 1;         // Position bar at bottom of screen?
	int force : 1;          // Force docking?
	int offset : 16;        // Offset between any two blocks 
	char *format;           // List and position of blocks
	char *block_font;	// The default font to use (slot 1)
	char *label_font;	// Font used for the label (slot 2)
	char *affix_font;	// Font used for prefix/suffix (slot 3)
	char *block_bg;         // Background color for all blocks
	char *label_fg;         // Foreground color for all labels
	char *label_bg;         // Background color for all labels
	char *affix_fg;         // Foreground color for all affixes
	char *affix_bg;         // Background color for all affixes
};

struct block
{
	char *name;             // Name of the block and its file/process
	char *path;             // Full path, including file name
	pid_t pid;		// Process ID of this block's process
	FILE *fd;               // File descriptor as returned by popen()
	char *fg;               // Foreground color
	char *bg;               // Background color
	char *label_fg;         // Foreground color for all labels
	char *label_bg;         // Background color for all labels
	char *affix_fg;         // Foreground color for all affixes
	char *affix_bg;         // Background color for all affixes
	char *lc;               // Overline/underline color
	int ol : 1;             // Draw overline?
	int ul : 1;             // Draw underline?
	size_t padding : 8;     // Minimum width of result in chars
	int offset : 16;        // Offset to next block in px
	int align;              // -1, 0, 1 (left, center, right)
	char *label;            // Prefixes the result string
	char *trigger;          // Run block based on this cmd
	char *cmd_lmb;          // Command to run on left mouse click
	char *cmd_mmb;          // Command to run on middle mouse click
	char *cmd_rmb;          // Command to run on right mouse click
	char *cmd_sup;          // Command to run on scroll up
	char *cmd_sdn;          // Command to run on scroll down
	int used : 1;           // Has this block been run at least once?
	double reload;          // Interval between runs 
	double waited;          // Time the block hasn't been run
	char *input;            // Recent output of the associated trigger
	char *result;           // Output of the most recent block run
};

struct trigger
{
	char *cmd;              // Command to run
	pid_t pid;              // Process ID of trigger command
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
	b->pid = 0;
	b->fd_in = NULL;
	b->fd_out = NULL;
	b->fg = NULL;
	b->bg = NULL;
	b->lc = NULL;
	b->lw = 1;
	b->ul = 0;
	b->ol = 0;
	b->w = 0;
	b->h = 0;
	b->x = 0;
	b->y = 0;
	b->bottom = 0;
	b->force = 0;
	b->offset = 0;
	b->prefix = NULL;
	b->suffix = NULL;
	b->format = NULL;
	b->block_font = NULL;
	b->label_font = NULL;
	b->affix_font = NULL;
	b->block_bg = NULL;
	b->label_fg = NULL;
	b->label_bg = NULL;
	b->affix_fg = NULL;
	b->affix_bg = NULL;
}

/*
 * Init the given block struct to a well defined state using sensible defaults.
 */
void init_block(struct block *b)
{
	b->name = NULL;
	b->path = NULL;
	b->pid = 0;
	b->fd = NULL;
	b->fg = NULL;
	b->bg = NULL;
	b->lc = NULL;
	b->ul = 0;
	b->ol = 0;
	b->label_fg = NULL;
	b->label_bg = NULL;
	b->affix_fg = NULL;
	b->affix_bg = NULL;
	b->padding = 0;
	b->offset = -1;
	b->align = 0;
	b->label = NULL;
	b->used = 0;
	b->trigger = NULL;
	b->cmd_lmb = NULL;
	b->cmd_mmb = NULL;
	b->cmd_rmb = NULL;
	b->cmd_sup = NULL;
	b->cmd_sdn = NULL;
	b->reload = 5.0;
	b->waited = 0.0;
	b->input = NULL;
	b->result = NULL;
}

/*
 * Frees all members of the given bar that need freeing and sets them to NULL.
 */
void free_bar(struct bar *b)
{
	free(b->fg);
	free(b->bg);
	free(b->lc);
	free(b->prefix);
	free(b->suffix);
	free(b->format);
	free(b->block_font);
	free(b->label_font);
	free(b->affix_font);
	free(b->block_bg);
	free(b->label_fg);
	free(b->label_bg);
	free(b->affix_fg);
	free(b->affix_bg);

	init_bar(b); // Sets all pointers to NULL, just in case
}

/*
 * Frees all members of the given block that need freeing and sets them to NULL.
 */
void free_block(struct block *b)
{
	free(b->name);
	free(b->path);
	free(b->fg);
	free(b->bg);
	free(b->lc);
	free(b->label_fg);
	free(b->label_bg);
	free(b->affix_fg);
	free(b->affix_bg);
	free(b->label);
	free(b->trigger);
	free(b->cmd_lmb);
	free(b->cmd_mmb);
	free(b->cmd_rmb);
	free(b->cmd_sup);
	free(b->cmd_sdn);
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

/*
 * Opens the process `cmd` similar to popen() but does not invoke a shell.
 * Instead, wordexp() is used to expand the given command, if necessary.
 * If successful, the process id of the new process is being returned and the 
 * given FILE pointers are set to streams that correspond to pipes for reading 
 * and writing to the child process, accordingly. On error, -1 is returned.
 */
pid_t popen2(const char *cmd, FILE *pipes[2])
{
	if (!cmd || !strlen(cmd))
	{
		return -1;
	}

	int pipe_write[2];
	int pipe_read[2];

	// [0] = read end, [1] = write end
	if ((pipe(pipe_write) < 0) || (pipe(pipe_read) < 0))
       	{
		return -1;
	}

	pid_t pid = fork();
	if (pid == -1)
	{
		return -1;
	}
	else if (pid == 0) // child
	{
		if (dup2(pipe_write[0], STDIN_FILENO) == -1)
		{
			return -1;
		}
		if (dup2(pipe_read[1], STDOUT_FILENO) == -1)
		{
			return -1;
		}
		if (dup2(pipe_read[1], STDERR_FILENO) == -1)
		{
			return -1;
		}

		close(pipe_write[1]);
		close(pipe_read[0]);

		wordexp_t p;
		if (wordexp(cmd, &p, 0) != 0)
		{
			return -1;
		}
		
		execvp(p.we_wordv[0], p.we_wordv); // TODO add error handling
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

pid_t popenr(const char *cmd, FILE *pipes[2])
{
	if (!cmd || !strlen(cmd))
	{
		return -1;
	}

	int pipe_stdout[2];
	int pipe_stderr[2];

	// [0] = read end of pipe, [1] = write end of pipe
	if ((pipe(pipe_stdout) < 0) || (pipe(pipe_stderr) < 0))
       	{
		return -1;
	}

	pid_t pid = fork();
	if (pid == -1)
	{
		return -1;
	}
	else if (pid == 0) // child
	{
		// redirect stdout to the write end of this pipe
		if (dup2(pipe_stdout[1], STDOUT_FILENO) == -1)
		{
			return -1;
		}
		// redirect stderr to the write end of this pipe
		if (dup2(pipe_stderr[1], STDERR_FILENO) == -1)
		{
			return -1;
		}

		close(pipe_stdout[0]); // child doesn't need read end
		close(pipe_stderr[0]); // child doesn't need read end

		wordexp_t p;
		if (wordexp(cmd, &p, 0) != 0)
		{
			return -1;
		}
		
		execvp(p.we_wordv[0], p.we_wordv); // TODO add error handling
		_exit(1);
	}
	else // parent
	{
		close(pipe_stdout[1]); // parent doesn't need write end
		close(pipe_stderr[1]); // parent doesn't need write end
		pipes[0] = fdopen(pipe_stdout[0], "r");
		pipes[1] = fdopen(pipe_stderr[0], "r");
		return pid;
	}
}


/*
 * Creates a font parameter string that can be used when running lemonbar.
 * If the given font is not set, fontstr() will return an empty string.
 * In any case, the string returnd is malloc'd, so remember to free it.
 */
char *fontstr(const char *font)
{
	char *str = NULL;

	if (font)
	{
		size_t len = strlen(font) + 6;
		str = malloc(len);
		snprintf(str, len, "-f \"%s\"", font);
	}
	else
	{
		str = malloc(1);
		str[0] = '\0';
	}
	return str;
}

/*
 * Runs the bar process and opens file descriptors for reading and writing.
 * Returns 0 on success, -1 if bar could not be started.
 */
int open_bar(struct bar *b)
{
	char w[8];
	char h[8];

	snprintf(w, 8, "%d", b->w);
	snprintf(h, 8, "%d", b->h);

	char *block_font = fontstr(b->block_font);
	char *label_font = fontstr(b->label_font);
	char *affix_font = fontstr(b->affix_font);

	size_t buf_len = 25;
	buf_len += strlen(BAR_PROCESS);
	buf_len += strlen(block_font);
	buf_len += strlen(label_font);
	buf_len += strlen(affix_font);
	buf_len += (16 + 16 + 27 + 4 + 4);

	char bar_cmd[buf_len];
	snprintf(bar_cmd, buf_len,
		"%s -g %sx%s+%d+%d -F%s -B%s -U%s -u%d %s %s %s %s %s", // 24+1
		BAR_PROCESS, // strlen
		(b->w > 0) ? w : "", // max 8
		(b->h > 0) ? h : "", // max 8
		b->x, // max 8
		b->y, // max 8
		(b->fg && strlen(b->fg)) ? b->fg : "-", // strlen, max 9
		(b->bg && strlen(b->bg)) ? b->bg : "-", // strlen, max 9
		(b->lc && strlen(b->lc)) ? b->lc : "-",	// strlen, max 9
		b->lw, // max 4
		(b->bottom) ? "-b" : "", // max 2
		(b->force)  ? "-d" : "", // max 2
		block_font, // strlen
		label_font, // strlen
		affix_font  // strlen
	);

	free(block_font);
	free(label_font);
	free(affix_font);

	printf("Bar command: (length %zu/%zu)\n\t%s\n", strlen(bar_cmd), buf_len, bar_cmd);

	FILE *fd[2];
	if (popen2(bar_cmd, fd) == -1)
	{
		return -1;
	}

	setlinebuf(fd[0]);
	setlinebuf(fd[1]);
	b->fd_in = fd[1];
	b->fd_out = fd[0];

	return 0;
}

/*
 * Runs a block and creates a file descriptor for reading.
 * Returns 0 on success, -1 if block could not be executed.
 * TODO: Should this function check if the block is already open?
 */
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
	return (b->fd == NULL) ? -1 : 0;
}

/*
 * Convenience function: simply runs open_block() for all blocks.
 * Returns the number of blocks that were successfully opened.
 */
int open_blocks(struct block *blocks, int num_blocks)
{
	int blocks_opened;
	for(int i=0; i<num_blocks; ++i)
	{
		blocks_opened += (open_block(&blocks[i]) == 0) ? 1 : 0;
	}
	return blocks_opened;
}

/*
 * Closes the given bar process by closing its file descriptors.
 * The descriptors will also be set to NULL after closing.
 */
void close_bar(struct bar *b)
{
	if (b->fd_in != NULL)
	{
		fclose(b->fd_in);
		b->fd_in = NULL;
	}
	if (b->fd_out != NULL)
	{
		fclose(b->fd_out);
		b->fd_out = NULL;
	}
}

/*
 * Closes the given block by closing its file descriptor.
 * The descriptor will also be set to NULL after closing.
 */
void close_block(struct block *b)
{
	if (b->fd != NULL)
	{
		pclose(b->fd);
		b->fd = NULL;
	}
}

/*
 * Convenience function: simply runs close_block() for all blocks.
 */
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
		return -1;
	}

	FILE *fd[2];
	t->pid = popenr(t->cmd, fd);
	if (t->pid == -1)
	{
		return -1;
	}

	t->fd = fd[0];  // stdout is what we're after
	fclose(fd[1]); // Not interested in stderr

	setlinebuf(t->fd); // TODO add error handling
	int fn = fileno(t->fd);
	int flags;
	flags = fcntl(fn, F_GETFL, 0); // TODO add error handling
	flags |= O_NONBLOCK;
	fcntl(fn, F_SETFL, flags); // TODO add error handling
	return 0;
}

/*
 * Closes the trigger by closing its file descriptor
 * and sending a SIGTERM to the trigger command.
 * Also sets the file descriptor to NULL.
 */
void close_trigger(struct trigger *t)
{
	if (t->pid > 1)
	{
		kill(t->pid, SIGTERM); // Politely ask to terminate
	}
	if (t->fd != NULL)
	{
		fclose(t->fd);
		t->fd = NULL;
	}
}

/*
 * Convenience function: simply opens all given triggers.
 * Returns the number of successfully opened triggers.
 */ 
int open_triggers(struct trigger *triggers, int num_triggers)
{
	int num_triggers_opened = 0;
	for (int i=0; i<num_triggers; ++i)
	{
		num_triggers_opened += (open_trigger(&triggers[i]) == 0) ? 1 : 0;
	}
	return num_triggers_opened;
}

/*
 * Convenience function: simply closes all given triggers.
 */
void close_triggers(struct trigger *triggers, int num_triggers)
{
	for (int i=0; i<num_triggers; ++i)
	{
		close_trigger(&triggers[i]);
	}
}

void free_trigger(struct trigger *t)
{
	free(t->cmd);
	t->cmd = NULL;

	t->b = NULL;
	t->bar = NULL;
}

/*
 * Convenience function: simply frees all given blocks.
 */
void free_blocks(struct block *blocks, int num_blocks)
{
	for (int i=0; i<num_blocks; ++i)
	{
		free_block(&blocks[i]);
	}
}

/*
 * Convenience function: simply frees all given triggers.
 */
void free_triggers(struct trigger *triggers, int num_triggers)
{
	for (int i=0; i<num_triggers; ++i)
	{
		free_trigger(&triggers[i]);
	}
}

/*
 * Executes the given block by calling open_block() on it and saves the output 
 * of the block, if any, in its `result` field. If the block was run for the 
 * first time, it will be marked as `used`. The `result_length` argument gives
 * the size of the buffer that will be used to fetch the block's output.
 * Returns 0 on success, -1 if the block could not be run or its output could
 * not be fetched.
 */
int run_block(struct block *b, size_t result_length)
{
	open_block(b);
	if (b->fd == NULL)
	{
		// printf("Block is dead: `%s`", b->name);
		return -1;
	}
	if (b->result != NULL)
	{
		free(b->result);
		b->result = NULL;
	}
	b->result = malloc(result_length);
	if (fgets(b->result, result_length, b->fd) == NULL)
	{
		// printf("Unable to fetch input from block: `%s`", b->name);
		close_block(b);
		return -1;
	}
	b->result[strcspn(b->result, "\n")] = 0; // Remove '\n'
	b->used = 1; // Mark this block as having run at least once
	b->waited = 0.0; // This block was last run... now!
	b->input = NULL; // Discard input, as we've processed it now
	close_block(b);
	return 0;
}

/*
 * Escapes % characters in the given string by prepending another % in front of them,
 * effectively doubling all %. The result is returned as a malloc'd string, so it is 
 * upon the caller to free the result at some point. If `diff` is not NULL, escape() 
 * will set it to the number of % chars found in `str`, effectively giving the 
 * difference in size between `str` and the result.
 */
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
 * Returns the second string, if not empty or NULL, otherwise the first.
 * If both are empty or NULL, the fallback is returned.
 */
const char *colorstr(const char *standard, const char *override,  const char *fallback)
{
	if (override && strlen(override))
	{
		return override;
	}
	if (standard && strlen(standard))
	{
		return standard;
	}
	return fallback;
}

/*
 * Given a block, it returns a pointer to a string that is the formatted result 
 * of this block's script output, ready to be fed to Lemonbar, including prefix,
 * label and suffix. The string is malloc'd and should be free'd by the caller.
 * If `len` is positive, it will be used as buffer size for the result string.
 * This means that `len` needs to be big enough to contain the fully formatted 
 * string this function is putting together, otherwise truncation will happen.
 * Alternatively, set `len` to 0 to let this function calculate the buffer.
 */
char *blockstr(const struct bar *bar, const struct block *block, size_t len)
{
	char action_start[(5 * strlen(block->name)) + 56]; // ... + (5 * 11) + 1
	action_start[0] = 0;
	char action_end[21]; // (5 * 4) + 1
	action_end[0] = 0;

	if (block->cmd_lmb)
	{
		strcat(action_start, "%{A1:");
		strcat(action_start, block->name);
		strcat(action_start, "_lmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->cmd_mmb)
	{
		strcat(action_start, "%{A2:");
		strcat(action_start, block->name);
		strcat(action_start, "_mmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->cmd_rmb)
	{
		strcat(action_start, "%{A3:");
		strcat(action_start, block->name);
		strcat(action_start, "_rmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->cmd_sup)
	{
		strcat(action_start, "%{A4:");
		strcat(action_start, block->name);
		strcat(action_start, "_sup:}");
		strcat(action_end, "%{A}");
	}
	if (block->cmd_sdn)
	{
		strcat(action_start, "%{A5:");
		strcat(action_start, block->name);
		strcat(action_start, "_sdn:}");
		strcat(action_end, "%{A}");
	}

	size_t diff;
	char *result = escape(block->result, &diff);
	int padding = block->padding + diff;

	size_t buf_len;

	if (len > 0)
	{
		// If len is given, we use that as buffer size
		buf_len = len;
	}
	else
	{
		// Required buffer mainly depends on the result and name of a block
		buf_len = 239; // format str = 100, known stuff = 138, '\0' = 1
		buf_len += strlen(action_start);
		buf_len += bar->prefix ? strlen(bar->prefix) : 0;
		buf_len += bar->suffix ? strlen(bar->suffix) : 0;
		buf_len += block->label ? strlen(block->label) : 0;
		buf_len += strlen(result);
	}

	const char *fg = colorstr(NULL, block->fg, NULL);
	const char *bg = colorstr(bar->block_bg, block->bg, NULL);
	const char *lc = colorstr(NULL, block->lc, NULL);
	const char *label_fg = colorstr(bar->label_fg, block->label_fg, fg);
	const char *label_bg = colorstr(bar->label_bg, block->label_bg, bg);
	const char *affix_fg = colorstr(bar->affix_fg, block->affix_fg, fg);
	const char *affix_bg = colorstr(bar->affix_bg, block->affix_bg, bg);
        const int offset = (block->offset >= 0) ? block->offset : bar->offset;	
	const int ol = block->ol ? 1 : (bar->ol ? 1 : 0);
	const int ul = block->ul ? 1 : (bar->ul ? 1 : 0);

	char *str = malloc(buf_len);
	snprintf(str, buf_len,
		"%s%%{O%d}%%{F%s}%%{B%s}%%{U%s}%%{%co%cu}"        // start:  21
		"%%{T3}%%{F%s}%%{B%s}%s"                          // prefix: 13
		"%%{T2}%%{F%s}%%{B%s}%s"                          // label:  13
		"%%{T1}%%{F%s}%%{B%s}%*s"                         // block:  13
		"%%{T3}%%{F%s}%%{B%s}%s"                          // suffix: 13
		"%%{T-}%%{F-}%%{B-}%%{U-}%%{-o-u}%s",             // end:    27
		// Start
		action_start,                                     // strlen
		offset,                                           // max 4
		fg ? fg : "-",                                    // strlen, max 9
		bg ? bg : "-",                                    // strlen, max 9
		lc ? lc : "-",                                    // strlen, max 9
		ol ? '+' : '-',                                   // 1
		ul ? '+' : '-',                                   // 1
		// Prefix
		affix_fg ? affix_fg : "-",                        // strlen, max 9
		affix_bg ? affix_bg : "-",		          // strlen, max 9
		bar->prefix ? bar->prefix : "",                   // strlen
		// Label
		label_fg ? label_fg : "-",                        // strlen, max 9
		label_bg ? label_bg : "-",                        // strlen, max 9
		block->label ? block->label : "",                 // strlen
		// Block
		fg ? fg : "-",                                    // strlen, max 9
		bg ? bg : "-",                                    // strlen, max 9
		padding,                                          // max 4
		result,                                           // strlen
		// Suffix
		affix_fg ? affix_fg : "-",                        // strlen, max 9
		affix_bg ? affix_bg : "-",                        // strlen, max 9
		bar->suffix ? bar->suffix : "",                   // strlen
		// End
		action_end                                        // 5*4
	);

	free(result);
	return str;
}

/*
 * Returns 'l', 'c' or 'r' for input values -1, 0 and 1 respectively.
 * For other input values, the behavior is undefined.
 */
char get_align(const int align)
{
	char a[] = {'l', 'c', 'r'};
	return a[align+1]; 
}

/*
 * Combines the results of all given blocks into a single string that can be fed
 * to Lemonbar. Returns a pointer to the string, allocated with malloc().
 */
char *barstr(const struct bar *bar, const struct block *blocks, size_t num_blocks)
{
	// Short blocks like temperatur, volume or battery info will usually use 
	// something in the range of 130 to 200 byte. So let's go with 256 byte.
	size_t bar_str_len = 256 * num_blocks;
	char *bar_str = malloc(bar_str_len);
	bar_str[0] = '\0';

	char align[5];
	int last_align = 0;

	for (int i=0; i<num_blocks; ++i)
	{
		char *block_str = blockstr(bar, &blocks[i], 0);
		size_t block_str_len = strlen(block_str);
		if (blocks[i].align != last_align)
		{
			last_align = blocks[i].align;
			snprintf(align, 5, "%%{%c}", get_align(last_align));
			strcat(bar_str, align);
		}
		// Let's check if this block string can fit in our buffer
		size_t free_len = bar_str_len - (strlen(bar_str) + 1);
		if (block_str_len > free_len)
		{
			// Let's make space for approx. two more blocks
			bar_str_len += 256 * 2; 
			bar_str = realloc(bar_str, bar_str_len);
		}
		strcat(bar_str, block_str);
		free(block_str);
	}
	strcat(bar_str, "\n");
	bar_str = realloc(bar_str, strlen(bar_str) + 1);
	return bar_str;
}

int feed_bar(struct bar *bar, struct block *blocks, size_t num_blocks, 
		double delta, double tolerance, double *next)
{
	if (bar->fd_in == NULL)
	{
		return -1;
	}

	int num_blocks_executed = 0;	
	double until_next = DBL_MAX;
	double idle_left;

	for(int i=0; i<num_blocks; ++i)
	{
		blocks[i].waited += delta;
		idle_left = blocks[i].reload - blocks[i].waited;

		if (!blocks[i].used || blocks[i].input || 
				(blocks[i].reload > 0.0 && idle_left < tolerance))
		{
			num_blocks_executed += 
				(run_block(&blocks[i], BUFFER_SIZE) == 0) ? 1 : 0;
		}

		idle_left = blocks[i].reload - blocks[i].waited; // Recalc!
		if (blocks[i].input == NULL && idle_left < until_next)
		{
			// If reload is 0, this block idles forever
			if (blocks[i].reload > 0.0)
			{
				until_next = (idle_left > 0.0) ? idle_left : 0.0;
			}
		}
	}
	*next = until_next;

	if (num_blocks_executed)
	{
		char *lemonbar_str = barstr(bar, blocks, num_blocks);
		if (DEBUG) { printf("%s", lemonbar_str); }
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
	if (format == NULL)
	{
		return -1;
	}
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
	if (equals(name, "lc") || equals(name, "line"))
	{
		bar->lc = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "line-width"))
	{
		bar->lw = atoi(value);
		return 1;
	}
	if (equals(name, "ol") || equals(name, "overline"))
	{
		bar->ol = equals(value, "true") ? 1 : 0;
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		bar->ul = equals(value, "true") ? 1 : 0;
		return 1;
	}
	if (equals(name, "h") || equals(name, "height"))
	{
		bar->h = atoi(value);
		return 1;
	}
	if (equals(name, "w") || equals(name, "width"))
	{
		bar->w = atoi(value);
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
	if (equals(name, "offset") || equals(name, "block-offset"))
	{
		bar->offset = atoi(value);
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
	if (equals(name, "format") || equals(name, "blocks"))
	{
		bar->format = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "font") || equals(name, "block-font"))
	{
		bar->block_font = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-font"))
	{
		bar->label_font = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-font"))
	{
		bar->affix_font = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "block-background") || equals(name, "block-bg"))
	{
		bar->block_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-background") || equals(name, "label-bg"))
	{
		bar->label_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-foreground") || equals(name, "label-fg"))
	{
		bar->label_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-background") || equals(name, "affix-bg"))
	{
		bar->affix_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-foreground") || equals(name, "affix-fg"))
	{
		bar->affix_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	return 0; // unknown section/name, error
}

/*
 * Tries to load the config file (succaderc) and then goes on to set up the bar
 * by setting its properties according to the values read from the config file.
 * Returns 0 on success, -1 if the config file could not be found or read.
 */
int configure_bar(struct bar *b, const char *config_dir)
{
	char rc[PATH_MAX];
	snprintf(rc, sizeof(rc), "%s/%src", config_dir, NAME);
	if (ini_parse(rc, bar_ini_handler, b) < 0)
	{
		return -1;
	}
	return 0;
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
	if (equals(name, "label-background") || equals(name, "label-bg"))
	{
		block->label_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-foreground") || equals(name, "label-fg"))
	{
		block->label_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-background") || equals(name, "affix-bg"))
	{
		block->affix_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-foreground") || equals(name, "affix-fg"))
	{
		block->affix_fg = is_quoted(value) ? unquote(value) : strdup(value);
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
			block->reload = 0.0;
		}
		else
		{
			block->reload = atof(value);
		}
		return 1;
	}
	if (equals(name, "trigger"))
	{
		block->trigger = is_quoted(value) ? unquote(value) : strdup(value);
		block->reload = 0.0;
		return 1;
	}
	if (equals(name, "mouse-left"))
	{
		block->cmd_lmb = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "mouse-middle"))
	{
		block->cmd_mmb = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "mouse-right"))
	{
		block->cmd_rmb = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "scroll-up"))
	{
		block->cmd_sup = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "scroll-down"))
	{
		block->cmd_sdn = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	return 0; // unknown section/name or error
}

/*
 * Load the configuration file (ini) for the given block, if it exists,
 * and have the ini reader (inih) call block_ini_handler() accordingly.
 * Returns 0 on success, -1 if the config file does'n exist or couldn't be read.
 */
int configure_block(struct block *b, const char *blocks_dir)
{
	char *blockini = filepath(blocks_dir, b->name, "ini");
	if (access(blockini, R_OK) == -1)
	{
		//printf("No block config found for: %s\n", b->name);
		return -1;
	}
	if (ini_parse(blockini, block_ini_handler, b) < 0)
	{
		//printf("Can't parse block INI: %s\n", blockini);
		free(blockini);
		return -1;
	}
	free(blockini);
	return 0;
}

/*
 * Convenience function: simply runs configure_block() for all blocks.
 * Returns the number of blocks that were successfully configured.
 */
int configure_blocks(struct block *blocks, int num_blocks, const char *blocks_dir)
{
	int num_blocks_configured = 0;
	for (int i=0; i<num_blocks; ++i)
	{
		num_blocks_configured += 
			(configure_block(&blocks[i], blocks_dir) == 0) ? 1 : 0;
	}
	return num_blocks_configured;
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

/*
 * Returns 1 if the given file is executable, 0 otherwise.
 */
int is_executable(const char *dir, const char *filename)
{
	char *file = filepath(dir, filename, NULL);
	struct stat sb;
	int is_exec = (stat(file, &sb) == 0 && sb.st_mode & S_IXUSR);
	free(file);
	return is_exec;
}

int probably_a_block(const char *dir, const char *filename)
{
	return !is_ini(filename) && !is_hidden(filename) && is_executable(dir, filename);
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
			.pid = 0,
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
		if (entry->d_type == DT_REG && probably_a_block(blockdir, entry->d_name))
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

/*
 * Returns a pointer to a string that holds the config dir we want to use.
 * This does not check if the dir actually exists. You need to check still.
 * The string is allocated with malloc() and needs to be freed by the caller.
 */
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

/*
 * Returns a pointer to a string that holds the blocks dir we want to use.
 * This does not check if the dir actually exists. You need to check still.
 * If configdir is NULL, this method will call config_dir() to get it.
 * The returned string is malloc'd, so remember to free it at some point.
 */
char *blocks_dir(const char *configdir)
{
	const char *cfg_dir = (configdir) ? configdir : config_dir();
	size_t dir_len = strlen(cfg_dir) + strlen(BLOCKS_DIR) + 2;
	char *blocks_dir = malloc(dir_len);
	snprintf(blocks_dir, dir_len, "%s/%s", cfg_dir, BLOCKS_DIR);
	return blocks_dir;
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

	char res[BUFFER_SIZE];
	int num_lines = 0;

	while (fgets(res, BUFFER_SIZE, t->fd) != NULL)
	{
		++num_lines;
	}
	
	if (num_lines)
	{
		if (t->b->input != NULL)
		{
			free(t->b->input);
			t->b->input = NULL;
		}
		t->b->input = strdup(res);
	}
	
	return num_lines;
}

/*
 * Run the given command, which we want to do when the user triggers 
 * an action of a clickable area that has a command associated with it.
 * Returns the pid of the spawned process or -1 if running it failed.
 */
pid_t run_cmd(const char *cmd)
{
	if (!cmd || !strlen(cmd))
	{
		return -1;
	}
	wordexp_t p;
	if (wordexp(cmd, &p, 0) != 0)
	{
		//printf("Could not parse the command:\n\t'%s'\n", cmd);
		return -1;
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

	return (res == 0 ? pid : -1);
}

/*
 * Takes a string that might represent an action that was registered with one 
 * of the blocks and tries to find the associated block. If found, the command
 * associated with the action will be executed.
 * Returns 0 on success, -1 if the string was not a recognized action command
 * or the block that the action belongs to could not be found.
 */
int process_action(const char *action, struct block *blocks, int num_blocks)
{
	size_t len = strlen(action);
	if (len < 5)
	{
		return -1;	// Can not be an action command, too short
	}

	char types[5][5] = {"_lmb", "_mmb", "_rmb", "_sup", "_sdn"};

	char type[5]; // For the type suffix, including the underscore
	snprintf(type, 5, "%s", action + len - 5); // Extract the suffix
	char block[len-4]; // For everything _before_ the suffix
	snprintf(block, len - 4, "%s", action); // Extract that first part

	// We check if the action type is valid (see types)
	int b = 0;
	int found = 0;
	for (; b < 5; ++b)
	{
		if (equals(type, types[b]))
		{
			found = 1;
			break;
		}
	}

	if (!found)
	{
		return -1;	// Not a recognized action type
	}

	// Now we go through all blocks and try to find the right one
	for (int i=0; i<num_blocks; ++i)
	{
		if (equals(blocks[i].name, block))
		{
			// Now to fire the right command for the action type
			switch (b) {
				case 0:
					run_cmd(blocks[i].cmd_lmb);
					break;
				case 1:
					run_cmd(blocks[i].cmd_mmb);
					break;
				case 2:
					run_cmd(blocks[i].cmd_rmb);
					break;
				case 3:
					run_cmd(blocks[i].cmd_sup);
					break;
				case 4:
					run_cmd(blocks[i].cmd_sdn);
					break;
			}
			return 0;
		}
	}

	return -1; // Could not find the block associated with the action
}

/*
 * Handles SIGINT signals (CTRL+C) by setting the static variable
 * `running` to 0, effectively ending the main loop, so that clean-up happens.
 */
void sigint_handler(int sig)
{
	running = 0;
	handled = sig;
}

int main(void)
{
	/*
	 * LOGGING
	 */

	FILE *log = fopen("/home/julien/.succadelog", "w");
	if (log == NULL)
	{
		fprintf(stderr, "Could not create log file\n");
	}
	fprintf(log, "Start of log\n");

	// Prevent zombie children during runtime
	struct sigaction sa_chld = {
		.sa_handler = SIG_IGN
	};
	if (sigaction(SIGCHLD, &sa_chld, NULL) == -1)
	{
		fprintf(stderr, "Failed to ignore children's signals\n");
		fprintf(log, "Failed to ignore children's signals\n");
	}

	// Make sure we still do clean-up on SIGINT (ctrl+c)
	// and similar signals that indicate we should quit.
	struct sigaction sa_int = {
		.sa_handler = &sigint_handler
	};
	if (sigaction(SIGINT, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGINT handler\n");
		fprintf(log, "Failed to register SIGINT handler\n");
	}
	if (sigaction(SIGQUIT, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGQUIT handler\n");
		fprintf(log, "Failed to register SIGQUIT handler\n");
	}
	if (sigaction (SIGTERM, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGTERM handler\n");
		fprintf(log, "Failed to register SIGTERM handler\n");
	}

	/*
	// Non-Posix way of making sure that we kill succade when
	// the parent process (usually a window manage or similar) dies.
	if (prctl(PR_SET_PDEATHSIG, SIGHUP) == -1)
	{
		fprintf(stderr, "Failed to register for parent's death signal.\n");
		fprintf(log, "Failed to register for parent's death signal.\n");
	}
	*/

	/*
	 * DIRECTORIES
	 */

	char *configdir = config_dir();
	printf("Config directory:\n\t%s\n", configdir);

	char *blocksdir = blocks_dir(configdir);
	printf("Blocks directory:\n\t%s\n", blocksdir);

	/*
	 * BAR
	 */

	struct bar lemonbar;
	init_bar(&lemonbar);

	if (configure_bar(&lemonbar, configdir) == -1)
	{
		fprintf(stderr, "Failed to load RC file: %src\n", NAME);
		fprintf(log, "Failed to load RC file: %src\n", NAME);
		exit(EXIT_FAILURE);
	}
	if (open_bar(&lemonbar) == -1)
	{
		fprintf(stderr, "Failed to open bar: %s\n", BAR_PROCESS);
		fprintf(log, "Failed to open bar: %s\n", BAR_PROCESS);
		exit(EXIT_FAILURE);
	}

	free(configdir);
	
	/*
	 * BLOCKS
	 */

	struct block *blocks_requested; 
	int num_blocks_requested = parse_format(lemonbar.format, &blocks_requested, blocksdir);

	if (num_blocks_requested < 0)
	{
		fprintf(stderr, "Could not figure out what blocks to load, stopping %s\n",
				NAME);
		fprintf(log, "Could not figure out what blocks to load, stopping %s\n",
				NAME);
		exit(EXIT_FAILURE);
	}

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

	free(blocksdir);

	if (num_blocks == 0)
	{
		fprintf(stderr, "No blocks loaded (%d requested), stopping %s.\n",
			       	num_blocks_requested, NAME);
		fprintf(log, "No blocks loaded (%d requested), stopping %s.\n",
			       	num_blocks_requested, NAME);
		exit(EXIT_FAILURE);
	}

	/*
	 * BAR TRIGGER - triggers when lemonbar spits something to stdout/stderr
	 */
	
	struct trigger bartrig = {
		.cmd = NULL,
		.fd = lemonbar.fd_out,
		.b = NULL,
		.bar = &lemonbar,
		.ready = 0
	};
	
	/*
	 * TRIGGERS - trigger when their respective commands output something
	 */

	struct trigger *triggers;
	int num_triggers = create_triggers(&triggers, blocks, num_blocks);

	printf("Triggers found: (%d total)\n\t", num_triggers);
	fprintf(log,"Triggers found: (%d total)\n\t", num_triggers);
	for (int i=0; i<num_triggers; ++i)
	{
		fprintf(log, "'%s' ", triggers[i].cmd);
		printf("'%s' ", triggers[i].cmd);
	}
	printf("\n");

	int num_triggers_opened = open_triggers(triggers, num_triggers);

	printf("Triggeres opened: (%d total)\n\t", num_triggers_opened);
	fprintf(log,"Triggeres opened: (%d total)\n\t", num_triggers_opened);
	for (int i=0; i<num_triggers; ++i)
	{
		printf("'%s' ", triggers[i].cmd ? triggers[i].cmd : "");
		fprintf(log, "'%s' ", triggers[i].cmd ? triggers[i].cmd : "");
	}
	printf("\n");

	/* 
	 * EVENTS - register our triggers with the system so we'll be notified
	 */

	int epfd = epoll_create(1);
	if (epfd < 0)
	{
		fprintf(stderr, "Could not create epoll file descriptor\n");
		fprintf(log, "Could not create epoll file descriptor\n");
		exit(EXIT_FAILURE);
	}

	// Let's first register all our triggers associated with blocks	 
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

	if (epctl_result)
	{
		fprintf(stderr, "%d trigger events could not be registered\n", -1 * epctl_result);
		fprintf(log, "%d trigger events could not be registered\n", -1 * epctl_result);
		
	}

	// Now let's also add the bar trigger
	struct epoll_event eev = { 0 };
	eev.data.ptr = &bartrig;
	eev.events = EPOLLIN | EPOLLET;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fileno(bartrig.fd), &eev))
	{
		fprintf(stderr, "Failed to register bar trigger - clickable areas will not work.\n");
		fprintf(log, "Failed to register bar trigger - clickable areas will not work.\n");
	}

	/*
	 * MAIN LOOP
	 */

	double now;
	double before = get_time();
	double delta;
	double wait = 0.0;

	struct epoll_event tev[num_triggers + 1];

	char bar_output[BUFFER_SIZE];
	bar_output[0] = '\0';

	running = 1;
	
	fprintf(log, "Start of main loop\n");

	while (running)
	{
		fprintf(log, "x ");

		now = get_time();
		delta = now - before;
		before = now;
		
		//printf("WAIT = %f\n", wait);

		// Wait for trigger input - at least bartrig is always present
		int num_events = epoll_wait(epfd, tev, num_triggers + 1, wait * 1000);

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

		// Let's see if Lemonbar produced any output
		if (bartrig.ready)
		{
			fgets(bar_output, BUFFER_SIZE, lemonbar.fd_out);
			bartrig.ready = 0;
		}

		// Let's process bar's output, if any
		if (strlen(bar_output))
		{
			if (process_action(bar_output, blocks, num_blocks) < 0)
			{
				// It wasn't a recognized command, so chances are
				// that is was some debug/error output of bar.
				fprintf(log, "Lemongbar: %s", bar_output);
				printf("Lemonbar: %s", bar_output);
			}
			bar_output[0] = '\0';
		}

		// Let's update bar!
		feed_bar(&lemonbar, blocks, num_blocks, delta, 0.1, &wait);
	}

	/*
	 * CLEAN UP
	 */

	fprintf(log, "Received signal %d\n", handled);

	fprintf(stderr, "succade is about to shutdown, performing clean-up...\n");
	fprintf(log, "succade is about to shutdown, performing clean-up...\n");

	fprintf(stderr, "\tclosing epoll file descriptor\n");
	close(epfd);

	fprintf(stderr, "\tclosing all blocks\n");
	close_blocks(blocks, num_blocks);

	fprintf(stderr, "\tfreeing all blocks\n");
	free_blocks(blocks, num_blocks);

	free(blocks);
	
	// This is where it used to hang, due to pclose() calling wait()
	fprintf(stderr, "\tclosing all triggers\n");
	close_triggers(triggers, num_triggers);

	fprintf(stderr, "\tfreeing all triggers\n");
	free_triggers(triggers, num_triggers);

	free(triggers);
	
	fprintf(stderr, "\tclosing bar trigger\n");
	close_trigger(&bartrig);

	fprintf(stderr, "\tfreeing bar trigger\n");
	free_trigger(&bartrig);

	fprintf(stderr, "\tclosing bar\n");
	close_bar(&lemonbar);

	fprintf(stderr, "\tfreeing bar\n");
	free_bar(&lemonbar);

	fprintf(stderr, "Clean-up finished, see you next time!\n");
	fprintf(log, "Clean-up finished, see you next time!\n");

	fclose(log);

	return EXIT_SUCCESS;
}
