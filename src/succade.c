#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <float.h>
#include <spawn.h>
#include <wordexp.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include "ini.h"
#include "succade.h"
#include "options.c"
#include "helpers.c"
//#include "execute.c" (or "process.c"?)

#define DEBUG 0 
#define NAME "succade"
#define BAR_PROCESS "lemonbar"
#define BUFFER_SIZE 2048
#define BLOCK_NAME_MAX 64

extern char **environ;         // Required to pass the env to child cmds
static volatile int running;   // Used to stop main loop in case of SIGINT
static volatile int handled;   // The last signal that has been handled 

/*
 * Init the given bar struct to a well defined state using sensible defaults.
 */

void init_bar(scd_lemon_s *lemon)
{
	lemon->lw = 1;
}

/*
 * Init the given block struct to a well defined state using sensible defaults.
 */
void init_block(scd_block_s *block)
{
	block->offset = -1;
	block->reload = 5.0;
}

/*
 * Frees all members of the given bar that need freeing.
 */
void free_bar(scd_lemon_s *lemon)
{
	free(lemon->fg);
	free(lemon->bg);
	free(lemon->lc);
	free(lemon->prefix);
	free(lemon->suffix);
	free(lemon->format);
	free(lemon->block_font);
	free(lemon->label_font);
	free(lemon->affix_font);
	free(lemon->block_bg);
	free(lemon->label_fg);
	free(lemon->label_bg);
	free(lemon->affix_fg);
	free(lemon->affix_bg);
}

/*
 * Frees all members of the given block that need freeing.
 */
void free_block(scd_block_s *block)
{
	free(block->name);
	free(block->cfg);
	free(block->bin);
	free(block->fg);
	free(block->bg);
	free(block->lc);
	free(block->label_fg);
	free(block->label_bg);
	free(block->affix_fg);
	free(block->affix_bg);
	free(block->label);
	free(block->trigger);
	free(block->cmd_lmb);
	free(block->cmd_mmb);
	free(block->cmd_rmb);
	free(block->cmd_sup);
	free(block->cmd_sdn);
	free(block->input);
	free(block->result);
}

/*
 * Opens the process `cmd` similar to popen() but does not invoke a shell.
 * Instead, wordexp() is used to expand the given command, if necessary.
 * If successful, the process id of the new process is being returned and the 
 * given FILE pointers are set to streams that correspond to pipes for reading 
 * and writing to the child process, accordingly. Hand in NULL for pipes that
 * should not be used. On error, -1 is returned. Note that the child process 
 * might have failed to execute the given `cmd` (and therefore ended exection); 
 * the return value of this function only indicates whether the child process 
 * was successfully forked or not.
 */
pid_t popen_noshell(const char *cmd, FILE **out, FILE **err, FILE **in)
{
	if (!cmd || !strlen(cmd))
	{
		return -1;
	}

	// 0 = read end of pipes, 1 = write end of pipes
	int pipe_stdout[2];
	int pipe_stderr[2];
	int pipe_stdin[2];

	if (out && (pipe(pipe_stdout) < 0))
	{
		return -1;
	}
	if (err && (pipe(pipe_stderr) < 0))
	{
		return -1;
	}
	if (in && (pipe(pipe_stdin) < 0))
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
		if (out)
		{
			if (dup2(pipe_stdout[1], STDOUT_FILENO) == -1)
			{
				_exit(-1);
			}
			close(pipe_stdout[0]); // child doesn't need read end
		}
		// redirect stderr to the write end of this pipe
		if (err)
		{
			if (dup2(pipe_stderr[1], STDERR_FILENO) == -1)
			{
				_exit(-1);
			}
			close(pipe_stderr[0]); // child doesn't need read end
		}
		// redirect stdin to the read end of this pipe
		if (in)
		{
			if (dup2(pipe_stdin[0], STDIN_FILENO) == -1)
			{
				_exit(-1);
			}
			close(pipe_stdin[1]); // child doesn't need write end
		}

		wordexp_t p;
		if (wordexp(cmd, &p, 0) != 0)
		{
			_exit(-1);
		}
	
		// Child process could not be run (errno has more info)	
		if (execvp(p.we_wordv[0], p.we_wordv) == -1)
		{
			_exit(-1);
		}
		_exit(1);
	}
	else // parent
	{
		if (out)
		{
			close(pipe_stdout[1]); // parent doesn't need write end
			*out = fdopen(pipe_stdout[0], "r");
		}
		if (err)
		{
			close(pipe_stderr[1]); // parent doesn't need write end
			*err = fdopen(pipe_stderr[0], "r");
		}
		if (in)
		{
			close(pipe_stdin[0]); // parent doesn't need read end
			*in = fdopen(pipe_stdin[1], "w");
		}
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
int open_bar(scd_lemon_s *b)
{
	if (b == NULL)
	{
		fprintf(stderr, "huh?\n");
	}

	char w[8]; // TODO hardcoded value
	char h[8];

	snprintf(w, 8, "%d", b->w);
	snprintf(h, 8, "%d", b->h);

	char *block_font = fontstr(b->block_font);
	char *label_font = fontstr(b->label_font);
	char *affix_font = fontstr(b->affix_font);

	size_t buf_len = 26; // TODO hardcoded value
	buf_len += strlen(b->bin);
	buf_len += b->name ? strlen(b->name) : 0;
	buf_len += strlen(block_font);
	buf_len += strlen(label_font);
	buf_len += strlen(affix_font);
	buf_len += (16 + 16 + 27 + 4 + 4 + 4); // TODO hardcoded value

	fprintf(stderr, "okay\n");
	char bar_cmd[buf_len];
	snprintf(bar_cmd, buf_len,
		"%s -g %sx%s+%d+%d -F%s -B%s -U%s -u%d %s %s %s %s %s %s%s%s", // 25+1
		b->bin, // strlen
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
		affix_font, // strlen
		(b->name) ? "-n\"" : "",   // max 3
		(b->name) ? b->name : "", // strlen
		(b->name) ? "\"" : ""      // max 1
	);

	free(block_font);
	free(label_font);
	free(affix_font);

	fprintf(stderr, "Bar command: (length %zu/%zu)\n\t%s\n", strlen(bar_cmd), buf_len, bar_cmd);

	b->pid = popen_noshell(bar_cmd, &(b->fd_out), NULL, &(b->fd_in));
	if (b->pid == -1)
	{
		return -1;
	}
	setlinebuf(b->fd_out);
	setlinebuf(b->fd_in);

	return 0;
}

/*
 * Runs a block and creates a file descriptor (stream) for reading.
 * Returns 0 on success, -1 if block could not be executed.
 * TODO: Should this function check if the block is already open?
 */
int open_block(scd_block_s *b)
{
	if (b->pid != -1)
	{
		fprintf(stderr, "Not opening block as it is already open: %s\n", b->name);
		return -1;
	}

	// If no binary given, just use the block's name
	if (b->bin == NULL)
	{
		b->bin = strdup(b->name);
	}

	char *cmd = NULL;
	size_t cmd_len = 0;
	if (b->input)
	{
		cmd_len = strlen(b->bin) + strlen(b->input) + 4;
		cmd = malloc(cmd_len);
		snprintf(cmd, cmd_len, "%s '%s'", b->bin, b->input);
	}
	else
	{
		cmd_len = strlen(b->bin) + 1;
		cmd = malloc(cmd_len);
		snprintf(cmd, cmd_len, "%s", b->bin);
	}

	b->pid = popen_noshell(cmd, &(b->fd), NULL, NULL);
	if (b->pid == -1)
	{
		free(cmd);
		return -1;
	}

	free(cmd);
	return 0;
}

/*
 * Convenience function: simply runs open_block() for all blocks.
 * Returns the number of blocks that were successfully opened.
 * TODO currently not in use, can we trash it?
 */
int open_blocks(scd_block_s *blocks, size_t num_blocks)
{
	int blocks_opened;
	for (size_t i = 0; i < num_blocks; ++i)
	{
		blocks_opened += (open_block(&blocks[i]) == 0) ? 1 : 0;
	}
	return blocks_opened;
}

/*
 * Closes the given bar by killing the process, closing its file descriptors
 * and setting them to NULL after.
 */
void close_bar(scd_lemon_s *b)
{
	if (b->pid > 1)
	{
		kill(b->pid, SIGKILL);
		b->pid = 0;
	}
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
 * Closes the given block by killing the process, closing its file descriptor
 * and settings them to NULL after.
 */
void close_block(scd_block_s *b)
{
	if (b->pid > 1)
	{
		kill(b->pid, SIGTERM);
		b->pid = 0;
	}
	if (b->fd != NULL)
	{
		fclose(b->fd);
		b->fd = NULL;
	}
}

/*
 * Convenience function: simply runs close_block() for all blocks.
 */
void close_blocks(scd_block_s *blocks, size_t num_blocks)
{
	for (size_t i = 0; i < num_blocks; ++i)
	{
		close_block(&blocks[i]);
	}
}

int open_trigger(scd_spark_s *t)
{
	if (t->cmd == NULL)
	{
		return -1;
	}
	
	t->pid = popen_noshell(t->cmd, &(t->fd), NULL, NULL);
	if (t->pid == -1)
	{
		return -1;
	}

	setlinebuf(t->fd); // TODO add error handling
	int fn = fileno(t->fd);
	int flags = fcntl(fn, F_GETFL, 0); // TODO add error handling
	flags |= O_NONBLOCK;
	fcntl(fn, F_SETFL, flags); // TODO add error handling
	return 0;
}

/*
 * Closes the trigger by closing its file descriptor
 * and sending a SIGTERM to the trigger command.
 * Also sets the file descriptor to NULL.
 */
void close_trigger(scd_spark_s *t)
{
	// Is the trigger's command still running?
	if (t->pid > 1)
	{
		kill(t->pid, SIGTERM); // Politely ask to terminate
	}
	// If bar is set, then fd is a copy and will be closed elsewhere
	if (t->bar)
	{
		return;
	}
	// Looks like we should actually close/free this fd after all
	if (t->fd)
	{
		fclose(t->fd);
		t->fd = NULL;
		t->pid = 0;
	}
}

/*
 * Convenience function: simply opens all given triggers.
 * Returns the number of successfully opened triggers.
 */ 
size_t open_triggers(scd_spark_s *triggers, size_t num_triggers)
{
	size_t num_triggers_opened = 0;
	for (size_t i = 0; i < num_triggers; ++i)
	{
		num_triggers_opened += (open_trigger(&triggers[i]) == 0) ? 1 : 0;
	}
	return num_triggers_opened;
}

/*
 * Convenience function: simply closes all given triggers.
 */
void close_triggers(scd_spark_s *triggers, size_t num_triggers)
{
	for (size_t i = 0; i < num_triggers; ++i)
	{
		close_trigger(&triggers[i]);
	}
}

void free_trigger(scd_spark_s *t)
{
	free(t->cmd);
	t->cmd = NULL;

	t->b = NULL;
	t->bar = NULL;
}

/*
 * Convenience function: simply frees all given blocks.
 */
void free_blocks(scd_block_s *blocks, size_t num_blocks)
{
	for (size_t i = 0; i < num_blocks; ++i)
	{
		free_block(&blocks[i]);
	}
}

/*
 * Convenience function: simply frees all given triggers.
 */
void free_triggers(scd_spark_s *triggers, size_t num_triggers)
{
	for (size_t i = 0; i < num_triggers; ++i)
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
int run_block(scd_block_s *b, size_t result_length)
{
	if (b->live)
	{
		fprintf(stderr, "Block is live: `%s`\n", b->name);
		return -1;
	}

	fprintf(stderr, "Attempting to open block `%s`\n", b->name);

	open_block(b);
	if (b->fd == NULL)
	{
		fprintf(stderr, "Block is dead: `%s`", b->name);
		close_block(b); // In case it has a PID already    TODO does this make sense?
		b->enabled = 0; // Prevent future attempts to open TODO does this make sense?
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
		fprintf(stderr, "Unable to fetch input from block: `%s`", b->name);
		close_block(b);
		return -1;
	}
	b->result[strcspn(b->result, "\n")] = 0; // Remove '\n'
	b->used = 1; // Mark this block as having run at least once
	b->waited = 0.0; // This block was last run... now!
	free(b->input);
	b->input = NULL; // Discard input, as we've processed it now
	close_block(b);
	return 0;
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
char *blockstr(const scd_lemon_s *bar, const scd_block_s *block, size_t len)
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
	char *result = escape(block->result, '%', &diff);
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
char *barstr(const scd_lemon_s *bar, const scd_block_s *blocks, size_t num_blocks)
{
	// Short blocks like temperatur, volume or battery info will usually use 
	// something in the range of 130 to 200 byte. So let's go with 256 byte.
	size_t bar_str_len = 256 * num_blocks; // TODO hardcoded value
	char *bar_str = malloc(bar_str_len);
	bar_str[0] = '\0';

	char align[5];
	int last_align = -1;

	for (int i = 0; i < num_blocks; ++i)
	{
		// Skip disabled blocks
		if (!blocks[i].enabled)
		{
			continue;
		}

		// TODO just quick hack to get this working
		if (blocks[i].bin == NULL)
		{
			fprintf(stderr, "Block binary not given for '%s', skipping\n", blocks[i].name);
			continue;
		}

		// Live blocks might not have a result available
		if (blocks[i].result == NULL)
		{
			continue;
		}

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

int feed_bar(scd_lemon_s *bar, scd_block_s *blocks, size_t num_blocks, 
		double delta, double tolerance, double *next)
{
	// Can't pipe to bar if its file descriptor isn't available
	if (bar->fd_in == NULL)
	{
		return -1;
	}

	int num_blocks_executed = 0;	
	double until_next = DBL_MAX;
	double idle_left;

	for (int i = 0; i < num_blocks; ++i)
	{
		// Skip blocks that aren't enabled
		if (!blocks[i].enabled)
		{
			continue;
		}

		// Skip live blocks, they will update based on their output
		//if (blocks[i].live && blocks[i].result)
		// ^-- why did we do the '&& block[i].result' thing?
		if (blocks[i].live)
		{
			// However, we count them as executed block so that
			// we actually end up updating the bar further down
			++num_blocks_executed;
			continue;
		}

		// Updated the time this block hasn't been run
		blocks[i].waited += delta;

		// Calc how long until this block should be run
		idle_left = blocks[i].reload - blocks[i].waited;

		// Block was never run before OR block has input waiting OR
		// it's time to run this block according to it's reload option
		if (!blocks[i].used || blocks[i].input || 
				(blocks[i].reload > 0.0 && idle_left < tolerance))
		{
			num_blocks_executed += 
				(run_block(&blocks[i], BUFFER_SIZE) == 0) ? 1 : 0;
		}

		idle_left = blocks[i].reload - blocks[i].waited; // Recalc!

		// Possibly update the time until we should run feed_bar again
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
		if (DEBUG) { fprintf(stderr, "%s", lemonbar_str); }
		fputs(lemonbar_str, bar->fd_in);
		free(lemonbar_str);
	}
	return num_blocks_executed;
}

/*
 * Quickly scan the 'format' string and return the number of block names in it.
 */
int scan_blocks(const char *format)
{
	int in_a_block = 0;
	size_t num_blocks = 0;
	size_t format_len = strlen(format) + 1;
	for (size_t i = 0; i < format_len; ++i)
	{
		switch (format[i])
		{
			case '|':
			case ' ':
			case '\0':
				num_blocks += in_a_block ? 1 : 0;
				in_a_block = 0;
				break;
			default:
				in_a_block = 1;	
		}
	}
	return num_blocks;
}

size_t parse_format_cb(const char *format, create_block_callback cb, void *data)
{
	if (format == NULL)
	{
		return 0;
	}

	size_t format_len = strlen(format) + 1;
	char block_name[BLOCK_NAME_MAX];
	block_name[0] = '\0';
	size_t block_name_len = 0;
	int block_align = -1;
	int num_blocks = 0;

	for (size_t i = 0; i < format_len; ++i)
	{
		switch (format[i])
		{
		case '|':
			// Next align
			block_align += block_align < 1;
		case ' ':
		case '\0':
			if (block_name_len)
			{
				// Block name complete, inform the callback
				cb(block_name, block_align, num_blocks++, data);
				// Prepare for the next block name
				block_name[0] = '\0';
				block_name_len = 0;
			}
			break;
		default:
			// Add the char to the current's block name
			block_name[block_name_len++] = format[i];
			block_name[block_name_len]   = '\0';
		}
	}

	// We inform the callback one last time, but set name = NULL
	//cb(NULL, 0, num_blocks, data);
	
	// Return the number of blocks found
	return num_blocks;
}

static int bar_ini_handler(void *b, const char *section, const char *name, const char *value)
{
	scd_lemon_s *bar = (scd_lemon_s*) b;
	if (equals(name, "name"))
	{
		bar->name = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bin"))
	{
		bar->bin = is_quoted(value) ? unquote(value) : strdup(value);
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
		bar->ol = equals(value, "true");
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		bar->ul = equals(value, "true");
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
		bar->bottom = equals(value, "bottom");
		return 1;
	}
	if (equals(name, "force"))
	{
		bar->force = equals(value, "true");
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

static int block_ini_handler(void *b, const char *section, const char *name, const char *value)
{
	scd_block_s *block = (scd_block_s*) b;
	if (equals(name, "bin"))
	{
		block->bin = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
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
		block->ol = equals(value, "true");
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		block->ul = equals(value, "true");
		return 1;
	}
	if (equals(name, "live"))
	{
		block->live = equals(value, "true");
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

scd_block_s *get_block(scd_block_s *blocks, size_t num_blocks, const char *name)
{
	// Iterate over all existing blocks and check for a name match
	for (size_t i = 0; i < num_blocks; ++i)
	{
		// If names match, return a pointer to this block
		if (strcmp(blocks[i].name, name) == 0)
		{
			return &blocks[i];
		}
	}
	return NULL;
}

/*
 * Add the block with the given name to the collection of blocks in the given 
 * block container, unless there is already a block with that name in there. 
 * Returns a pointer to the added (or existing) block or NULL in case of error.
 */
scd_block_s *add_block(scd_state_s *state, const char *name)
{
	// See if there is an existing block by this name (and return, if so)
	scd_block_s *eb = get_block(state->blocks, state->num_blocks, name);
	if (eb)
	{
		return eb;
	}

	// Resize the block container to be able to hold one more block
	int current = state->num_blocks;
	state->num_blocks += 1;
	state->blocks = realloc(state->blocks, sizeof(scd_block_s) * state->num_blocks);
	
	// Create the block, setting its name and default values
	state->blocks[current] = (scd_block_s) { .name = strdup(name) };
	init_block(&state->blocks[current]);

	// Return a pointer to the new block
	return &state->blocks[current];
}

int cfg_handler(void *data, const char *section, const char *name, const char *value)
{
	scd_state_s *state = (scd_state_s*) data;

	// Return early if no section given (user/config error)
	if (section[0] == '\0')
	{
		return 0;
	}
	// Call the bar config handler for the special section "bar"
	if (strcmp(section, "bar") == 0)
	{
		return bar_ini_handler(state->lemon, section, name, value);
	}
	// Call the block config handler for any other section
	else
	{
		fprintf(stderr, "Calling block config handler for %s\n", section);
		scd_block_s *block = add_block(state, section);
		if (block == NULL)
		{
			fprintf(stderr, "Could not create block %s - out of memory?\n", section);
			return 0;
		}
		return block_ini_handler(block, section, name, value);
	}
}

/*
 * Tries to load the config file (succaderc) and then goes on to set up the bar (and blocks)
 * by setting its properties according to the values read from the config file.
 * Returns 0 on success, -1 if the config file could not be found or read.
 */
int load_config(scd_state_s *state)
{
	return (ini_parse(state->prefs->config, cfg_handler, state) < 0) ? -1 : 0;
}

size_t create_sparks(scd_state_s *state)
{
	// No need for sparks if there aren't any blocks
	if (state->num_blocks == 0)
	{
		return 0;
	}

	// Use number of blocks as initial size 
	state->sparks = malloc(sizeof(scd_spark_s) * state->num_blocks);

	// Go through all blocks, create sparks as appropriate
	size_t num_sparks_created = 0;
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		// Trigger disabled blocks
		if (!state->blocks[i].enabled)
		{
			continue;
		}

		// Block that's triggered by another program's output
		if (state->blocks[i].trigger)
		{
			state->sparks[i] = (scd_spark_s) { 0 };
			state->sparks[i].cmd = strdup(state->blocks[i].trigger);
			state->sparks[i].b   = &state->blocks[i];
			num_sparks_created += 1;
			continue;
		}

		// Block that triggers itself ('live' block)
		if (state->blocks[i].live)
		{
			state->sparks[i] = (scd_spark_s) { 0 };
			state->sparks[i].cmd = strdup(state->blocks[i].bin);
			state->sparks[i].b   = &state->blocks[i];
			num_sparks_created += 1;
			continue;
		}
	}

	// Resize to whatever amount of memory we actually needed
	state->sparks = realloc(state->sparks, sizeof(scd_spark_s) * num_sparks_created);
	return num_sparks_created;
}

/*
 * Read all pending lines from the given trigger and store only the last line 
 * in the corresponding block's input field. Previous lines will be discarded.
 * Returns the number of lines read from the trigger's file descriptor.
 */
int run_trigger(scd_spark_s *t)
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
		// For live blocks, this will be the actual output
		if (t->b->live)
		{
			if (t->b->result != NULL)
			{
				free(t->b->result);
				t->b->result = NULL;
			}
			t->b->result = strdup(res);
			// Remove '\n'
			t->b->result[strcspn(t->b->result, "\n")] = 0;
		}
		// For regular blocks, this will be input for the block
		else
		{
			if (t->b->input != NULL)
			{
				free(t->b->input);
				t->b->input = NULL;
			}
			t->b->input = strdup(res);
		}
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
	// Return early if cmd is NULL or empty
	if (cmd == NULL || !strlen(cmd))
	{
		return -1;
	}

	// Try to parse the command (expand symbols like . and ~ etc)
	wordexp_t p;
	if (wordexp(cmd, &p, 0) != 0)
	{
		return -1;
	}
	
	// Spawn a new child process with the given command
	pid_t pid;
	int res = posix_spawnp(&pid, p.we_wordv[0], NULL, NULL, p.we_wordv, environ);
	wordfree(&p);
	
	// Return the child's PID on success, -1 on failure
	return (res == 0 ? pid : -1);
}

/*
 * Takes a string that might represent an action that was registered with one 
 * of the blocks and tries to find the associated block. If found, the command
 * associated with the action will be executed.
 * Returns 0 on success, -1 if the string was not a recognized action command
 * or the block that the action belongs to could not be found.
 */
int process_action(const char *action, scd_block_s *blocks, size_t num_blocks)
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
	for (int i = 0; i < num_blocks; ++i)
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

void found_block_handler(const char *name, int align, int n, void *data)
{
	// 'Unpack' the data
	scd_state_s *state = (scd_state_s*) data;
	
	// Find or add the block with the given name
	scd_block_s *block = add_block(state, name);
	
	// Mark the block as enabled 
	block->enabled = 1;

	// Set the block's align to the given one
	block->align = align;
}

// http://courses.cms.caltech.edu/cs11/material/general/usage.html
void help(const char *invocation)
{
	fprintf(stderr, "USAGE\n");
	fprintf(stderr, "\t%s [OPTIONS...]\n", invocation);
	fprintf(stderr, "\n");
	fprintf(stderr, "OPTIONS\n");
	fprintf(stderr, "\t-e\n");
	fprintf(stderr, "\t\tRun bar even if it is empty (no blocks).\n");
	fprintf(stderr, "\t-h\n");
	fprintf(stderr, "\t\tPrint this help text and exit.\n");
}

int main(int argc, char **argv)
{
	/*
	 * SIGNALS
	 */

	// Prevent zombie children during runtime
	// https://en.wikipedia.org/wiki/Child_process#End_of_life
	
	// TODO However, maybe it would be a good idea to handle
	//      this signal, as children will send it to the parent
	//      process when they exit; so we could use this to detect
	//      blocks that have died (prematurely/unexpectedly), for 
	//      example those that failed the `execvp()` call from 
	//      within `fork()` in `popen_noshell()`... not sure.
	struct sigaction sa_chld = {
		.sa_handler = SIG_IGN
	};
	if (sigaction(SIGCHLD, &sa_chld, NULL) == -1)
	{
		fprintf(stderr, "Failed to ignore children's signals\n");
	}

	// Make sure we still do clean-up on SIGINT (ctrl+c)
	// and similar signals that indicate we should quit.
	struct sigaction sa_int = {
		.sa_handler = &sigint_handler
	};
	if (sigaction(SIGINT, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGINT handler\n");
	}
	if (sigaction(SIGQUIT, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGQUIT handler\n");
	}
	if (sigaction (SIGTERM, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGTERM handler\n");
	}
	if (sigaction (SIGPIPE, &sa_int, NULL) == -1)
	{
		fprintf(stderr, "Failed to register SIGPIPE handler\n");
	}

	fprintf(stderr, "I'm alive #1\n");

	/*
	 * PARSE COMMAND LINE ARGUMENTS
	 */

	scd_state_s state = { 0 };
	scd_prefs_s prefs = { 0 };
	
	state.prefs = &prefs;
	parse_args(argc, argv, state.prefs);

	fprintf(stderr, "I'm alive #2\n");

	/*
	 * PRINT HELP AND EXIT, IF REQUESTED
	 */

	if (prefs.help)
	{
		help(argv[0]);
		return EXIT_SUCCESS;
	}

	/*
	 * CHECK IF X IS RUNNING
	 */

	char *display = getenv("DISPLAY");
	if (!display)
	{
		fprintf(stderr, "DISPLAY environment variable not set\n");
		return EXIT_FAILURE;
	}
	if (!strstr(display, ":"))
	{
		fprintf(stderr, "DISPLAY environment variable seems invalid\n");
		return EXIT_FAILURE;
	}
	fprintf(stderr, "DISPLAY env var:\n\t%s\n", display);

	/*
	 * DIRECTORIES
	 */

	// If no custom config file given, set it to the default
	if (prefs.config == NULL)
	{
		// TODO If we get this string from args, we don't need to free
		//      it but if we get it via config_path(), then we DO need 
		//      to free it. That's an issue as there is no way, later,
		//      to know where it came from. Should we simply strdup() 
		//      everything that comes in via args, so that we get some
		//      consistency? That would waste some memory though...
		prefs.config = config_path("succaderc");
	}

	/*
	 * BAR & BLOCKS
	 */

	scd_lemon_s lemonbar = { 0 };
	// lemonbar.name = state.prefs->binary ? state.prefs->binary : BAR_PROCESS;
	init_bar(&lemonbar);
	
	// Add references to the bar and blocks structs to the config struct
	state.lemon = &lemonbar;

	if (load_config(&state) == -1)
	{
		fprintf(stderr, "Failed to load config file: %s\n", prefs.config);
		return EXIT_FAILURE;
	}

	fprintf(stderr, "I'm alive #1\n");

	if (open_bar(&lemonbar) == -1)
	{
		fprintf(stderr, "Failed to open bar: %s\n", lemonbar.name);
		return EXIT_FAILURE;
	}

	// Parse the format string and call found_block_handler for every block
	size_t num_blocks_parsed = parse_format_cb(lemonbar.format, found_block_handler, &state);

	// Check how many blocks we _actually_ have (some might not have been
	// specified in the bar's 'format' string, yet had config sections)
	// and debug-print them at the same time
	size_t num_blocks_enabled = 0;
	fprintf(stderr, "Blocks found: (%zu total)\n\t", state.num_blocks);
	for (size_t i = 0; i < state.num_blocks; ++i)
	{
		num_blocks_enabled += state.blocks[i].enabled;
		fprintf(stderr, "%s ", state.blocks[i].name);
	}
	fprintf(stderr, "\n");

	fprintf(stderr, "Number of blocks: parsed = %zu, configured = %zu, enabled = %zu\n", 
			num_blocks_parsed, state.num_blocks, num_blocks_enabled);

	if (DEBUG)
	{
		fprintf(stderr, "Blocks found: (%zu total)\n\t", state.num_blocks);
		for (size_t i = 0; i < state.num_blocks; ++i)
		{
			fprintf(stderr, "%s ", state.blocks[i].name);
		}
		fprintf(stderr, "\n");
	}

	// Exit if no blocks could be loaded and 'empty' option isn't present
	if (num_blocks_enabled == 0 && prefs.empty == 0)
	{
		fprintf(stderr, "No blocks loaded, stopping %s.\n", NAME);
		return EXIT_FAILURE;
	}

	/*
	 * BAR TRIGGER - triggers when lemonbar spits something to stdout/stderr
	 */
	
	scd_spark_s bartrig = { 0 };
	bartrig.fd  =  lemonbar.fd_out;
	bartrig.bar = &lemonbar;
	
	/*
	 * TRIGGERS - trigger when their respective commands produce output
	 */

	//scd_spark_s *triggers;
	//size_t num_triggers = create_triggers(&triggers, state.blocks, state.num_blocks);
	size_t num_triggers = create_sparks(&state);
	scd_spark_s *triggers = state.sparks;

	// Debug-print all triggers that we found
	if (DEBUG)
	{
		fprintf(stderr, "Triggers found: (%zu total)\n\t", num_triggers);
		for (size_t i = 0; i < num_triggers; ++i)
		{
			fprintf(stderr, "'%s' ", triggers[i].cmd);
		}
		fprintf(stderr, "\n");
	}

	size_t num_triggers_opened = open_triggers(triggers, num_triggers);

	// Debug-print all triggers that we opened
	if (DEBUG)
	{
		fprintf(stderr, "Triggeres opened: (%zu total)\n\t", num_triggers_opened);
		for (size_t i = 0; i < num_triggers; ++i)
		{
			fprintf(stderr, "'%s' ", triggers[i].cmd ? triggers[i].cmd : "");
		}
		fprintf(stderr, "\n");
	}

	/* 
	 * EVENTS - register our triggers with the system so we'll be notified
	 */

	int epfd = epoll_create(1);
	if (epfd < 0)
	{
		fprintf(stderr, "Could not create epoll file descriptor\n");
		return EXIT_FAILURE;
	}

	// Let's first register all our triggers associated with blocks	 
	int epctl_result = 0;
	for (int i = 0; i < num_triggers; ++i)
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
	}

	// Now let's also add the bar trigger
	struct epoll_event eev = { 0 };
	eev.data.ptr = &bartrig;
	eev.events = EPOLLIN | EPOLLET;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fileno(bartrig.fd), &eev))
	{
		fprintf(stderr, "Failed to register bar trigger - clickable areas will not work.\n");
	}

	/*
	 * MAIN LOOP
	 */

	double now;
	double before = get_time();
	double delta;
	double wait = 0.0; // Will later be set to suitable value by feed_bar()

	struct epoll_event tev[num_triggers + 1];

	char bar_output[BUFFER_SIZE];
	bar_output[0] = '\0';

	running = 1;
	
	while (running)
	{
		now    = get_time();
		delta  = now - before;
		before = now;
		
		// Wait for trigger input - at least bartrig is always present
		int num_events = epoll_wait(epfd, tev, num_triggers + 1, wait * 1000);

		// Mark triggers that fired as ready to be read
		for (int i = 0; i < num_events; ++i)
		{
			if (tev[i].events & EPOLLIN)
			{
				((scd_spark_s*) tev[i].data.ptr)->ready = 1;

				scd_spark_s *t = tev[i].data.ptr;
				fprintf(stderr, "Trigger `%s` has activity!\n", t->cmd);
			}
		}	

		// Fetch input from all marked triggers
		for (int i = 0; i < num_triggers; ++i)
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
			if (process_action(bar_output, state.blocks, state.num_blocks) < 0)
			{
				// It wasn't a recognized command, so chances are
				// that is was some debug/error output of bar.
				// TODO just use stderr in addition to stdout
				fprintf(stderr, "Lemonbar: %s", bar_output);
			}
			bar_output[0] = '\0';
		}

		// Let's update bar! TODO hardcoded value (tolerance = 0.01)
		feed_bar(&lemonbar, state.blocks, state.num_blocks, delta, 0.1, &wait);
	}

	/*
	 * CLEAN UP
	 */

	fprintf(stderr, "Performing clean-up ...\n");
	close(epfd);

	// This is where it used to hang, due to pclose() calling wait()

	// Close triggers - it's important we free these first as they might
	// point to instances of bar and/or blocks, which will lead to errors
	close_triggers(triggers, num_triggers);

	free_triggers(triggers, num_triggers);

	free(triggers);
	
	close_trigger(&bartrig);

	free_trigger(&bartrig);

	// Close blocks
	close_blocks(state.blocks, state.num_blocks);

	free_blocks(state.blocks, state.num_blocks);

	free(state.blocks);

	// Close bar
	close_bar(&lemonbar);

	free_bar(&lemonbar);

	fprintf(stderr, "Clean-up finished, see you next time!\n");

	return EXIT_SUCCESS;
}

