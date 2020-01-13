#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <string.h>    // strlen(), strcmp(), ...
#include <signal.h>    // sigaction(), ... 
#include <fcntl.h>     // fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <float.h>     // DBL_MAX
#include <sys/epoll.h> // epoll_wait(), ... 
#include "ini.h"       // https://github.com/benhoyt/inih
#include "succade.h"   // defines, structs, all that stuff
#include "options.c"   // Command line args/options parsing
#include "helpers.c"   // Helper functions, mostly for strings
#include "execute.c"   // Execute child processes
#include "loadini.c"   // Handles loading/processing of INI cfg file

extern char **environ;         // Required to pass the env to child cmds
static volatile int running;   // Used to stop main loop in case of SIGINT
static volatile int handled;   // The last signal that has been handled 

/*
 * Init the given bar struct to a well defined state using sensible defaults.
 */

static void init_bar(scd_lemon_s *lemon)
{
	lemon->lw = 1;
}

/*
 * Init the given block struct to a well defined state using sensible defaults.
 */
static void init_block(scd_block_s *block)
{
	block->offset = -1;
	block->reload = 5.0;
}

/*
 * Frees all members of the given bar that need freeing.
 */
static void free_bar(scd_lemon_s *lemon)
{
	free(lemon->name);
	free(lemon->bin);
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
static void free_block(scd_block_s *block)
{
	free(block->name);
	free(block->bin);
	free(block->fg);
	free(block->bg);
	free(block->lc);
	free(block->label_fg);
	free(block->label_bg);
	free(block->affix_fg);
	free(block->affix_bg);
	free(block->label);
	free(block->spark);
	free(block->cmd_lmb);
	free(block->cmd_mmb);
	free(block->cmd_rmb);
	free(block->cmd_sup);
	free(block->cmd_sdn);
	free(block->input);
	free(block->result);
}

/*
 * Runs the bar process and opens file descriptors for reading and writing.
 * Returns 0 on success, -1 if bar could not be started.
 */
int open_bar(scd_lemon_s *b)
{
	char w[8]; // TODO hardcoded value
	char h[8];

	snprintf(w, 8, "%d", b->w);
	snprintf(h, 8, "%d", b->h);

	char *block_font = optstr('f', b->block_font);
	char *label_font = optstr('f', b->label_font);
	char *affix_font = optstr('f', b->affix_font);
	char *name_str   = optstr('n', b->name);

	size_t buf_len = 26; // TODO hardcoded value
	buf_len += strlen(b->bin);
	buf_len += strlen(name_str);
	buf_len += strlen(block_font);
	buf_len += strlen(label_font);
	buf_len += strlen(affix_font);
	buf_len += (16 + 16 + 27 + 4 + 4); // TODO hardcoded value

	char bar_cmd[buf_len];
	snprintf(bar_cmd, buf_len,
		"%s -g %sx%s+%d+%d -F%s -B%s -U%s -u%d %s %s %s %s %s %s", // 25+1
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
		name_str    // strlen
	);

	free(block_font);
	free(label_font);
	free(affix_font);
	free(name_str);

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
	if (t->lemon)
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

	t->block = NULL;
	t->lemon = NULL;
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
		fprintf(stderr, "Block is dead: `%s`\n", b->name);
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

	const char *fg = strsel(block->fg, NULL, NULL);
	const char *bg = strsel(block->bg, bar->block_bg, NULL);
	const char *lc = strsel(block->lc, NULL, NULL);
	const char *label_fg = strsel(block->label_fg, bar->label_fg, fg);
	const char *label_bg = strsel(block->label_bg, bar->label_bg, bg);
	const char *affix_fg = strsel(block->affix_fg, bar->affix_fg, fg);
	const char *affix_bg = strsel(block->affix_bg, bar->affix_bg, bg);
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
char *barstr(const scd_state_s *state)
{
	// For convenience...
	const scd_lemon_s *bar = state->lemon;
	const scd_block_s *blocks = state->blocks;
	size_t num_blocks = state->num_blocks;

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

/*
 * TODO add comment, possibly some refactoring
 */
size_t feed_bar(scd_state_s *state, double delta, double tolerance, double *next)
{

	// Can't pipe to bar if its file descriptor isn't available
	if (state->lemon->fd_in == NULL)
	{
		return -1;
	}
	
	// For convenience...
	scd_lemon_s *bar = state->lemon;
	scd_block_s *blocks = state->blocks;
	size_t num_blocks = state->num_blocks;

	size_t num_blocks_executed = 0;	
	double until_next = DBL_MAX;
	double idle_left;

	for (size_t i = 0; i < num_blocks; ++i)
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
		char *lemonbar_str = barstr(state);
		if (DEBUG) { fprintf(stderr, "%s", lemonbar_str); }
		fputs(lemonbar_str, bar->fd_in);
		free(lemonbar_str);
	}
	return num_blocks_executed;
}

/*
 * TODO add comment, possibly refactor some
 */
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

/*
 * Finds and returns the block with the given `name` -- or NULL.
 */
scd_block_s *get_block(const scd_state_s *state, const char *name)
{
	// Iterate over all existing blocks and check for a name match
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		// If names match, return a pointer to this block
		if (strcmp(state->blocks[i].name, name) == 0)
		{
			return &state->blocks[i];
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
	//scd_block_s *eb = get_block(state->blocks, state->num_blocks, name);
	scd_block_s *eb = get_block(state, name);
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

/*
 * This callback function will be called by ini_parse() whenever it read a new 
 * name-value-pair from the given INI config file. The job of this function is
 * to figure out if the configuration option was for the bar itself or one of 
 * the blocks, then forward to the appropriate ini handler accordingly. 
 * Additionally, this function will create a new block in case the given config 
 * option refers to a block that doesn't exist yet.
 * TODO ^- read that last sentence... I don't much like this design.
 *         Separation of concerns and shit, you know? But I don't see how else
 *         we can do it at the moment, as there is no guarantee that the bar's
 *         'format' option will be read before any of the other sections; we 
 *         have tested that already.  
 */
int cfg_handler(void *data, const char *section, const char *name, const char *value)
{
	scd_state_s *state = (scd_state_s*) data;

	// No section means we assume this is for the bar itself then
	if (section[0] == '\0')
	{
		return lemon_ini_handler(state->lemon, section, name, value);
	}
	
	// Call the bar config handler for the special section "bar"
	if (strcmp(section, "bar") == 0)
	{
		return lemon_ini_handler(state->lemon, section, name, value);
	}
	
	// Call the block config handler for any other section
	else
	{
		scd_block_s *block = add_block(state, section);
		if (block == NULL)
		{
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
	size_t num_sparks = 0;
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		// Trigger disabled blocks
		if (!state->blocks[i].enabled)
		{
			continue;
		}

		// Block that's triggered by another program's output
		if (state->blocks[i].spark)
		{
			state->sparks[num_sparks] = (scd_spark_s) { 0 };
			state->sparks[num_sparks].cmd = strdup(state->blocks[i].spark);
			state->sparks[num_sparks].block = &state->blocks[i];
			num_sparks += 1;
			continue;
		}

		// Block that triggers itself ('live' block)
		if (state->blocks[i].live)
		{
			state->sparks[num_sparks] = (scd_spark_s) { 0 };
			state->sparks[num_sparks].cmd = strdup(state->blocks[i].bin);
			state->sparks[num_sparks].block = &state->blocks[i];
			num_sparks += 1;
			continue;
		}
	}


	// Resize to whatever amount of memory we actually needed
	state->sparks = realloc(state->sparks, sizeof(scd_spark_s) * num_sparks);
	state->num_sparks = num_sparks;
	return num_sparks;
}

/*
 * Read all pending lines from the given trigger and store only the last line 
 * in the corresponding block's input field. Previous lines will be discarded.
 * Returns the number of lines read from the trigger's file descriptor.
 */
size_t run_spark(scd_spark_s *t)
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
		if (t->block->live)
		{
			if (t->block->result != NULL)
			{
				free(t->block->result);
				t->block->result = NULL;
			}
			t->block->result = strdup(res);
			// Remove '\n'
			t->block->result[strcspn(t->block->result, "\n")] = 0;
		}
		// For regular blocks, this will be input for the block
		else
		{
			if (t->block->input != NULL)
			{
				free(t->block->input);
				t->block->input = NULL;
			}
			t->block->input = strdup(res);
		}
	}
	
	return num_lines;
}

/*
 * Takes a string that might represent an action that was registered with one 
 * of the blocks and tries to find the associated block. If found, the command
 * associated with the action will be executed.
 * Returns 0 on success, -1 if the string was not a recognized action command
 * or the block that the action belongs to could not be found.
 */
int process_action(const scd_state_s *state, const char *action)
{
	size_t len = strlen(action);
	if (len < 5)
	{
		return -1;	// Can not be an action command, too short
	}

	// A valid action command should have the format <blockname>_<cmd-type>
	// For example, for a block named `datetime` that was clicked with the 
	// left mouse button, `action` should be "datetime_lmb"

	char types[5][5] = {"_lmb", "_mmb", "_rmb", "_sup", "_sdn"};

	// Extract the type suffix, including the underscore
	char type[5]; 
	snprintf(type, 5, "%s", action + len - 5);

	// Extract everything _before_ the suffix (this is the block name)
	char block[len-4];
	snprintf(block, len - 4, "%s", action); 

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

	// Not a recognized action type
	if (!found)
	{
		return -1;
	}

	// Find the source block of the action
	scd_block_s *source = get_block(state, block);
	if (source == NULL)
	{
		return -1;
	}

	// Now to fire the right command for the action type
	switch (b) {
		case 0:
			run_cmd(source->cmd_lmb);
			return 0;
		case 1:
			run_cmd(source->cmd_mmb);
			return 0;
		case 2:
			run_cmd(source->cmd_rmb);
			return 0;
		case 3:
			run_cmd(source->cmd_sup);
			return 0;
		case 4:
			run_cmd(source->cmd_sdn);
			return 0;
		default:
			// Should never happen...
			return -1;
	}
}

/*
 * This callback is supposed to be called for every block name that is being 
 * extracted from the config file's 'format' option for the bar itself, which 
 * lists the blocks to be displayed on the bar. `name` should contain the name 
 * of the block as read from the format string, `align` should be -1, 0 or 1, 
 * meaning left, center or right, accordingly (indicating where the block is 
 * supposed to be displayed on the bar).
 */
static void found_block_handler(const char *name, int align, int n, void *data)
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

/*
 * Handles SIGINT signals (CTRL+C) by setting the static variable
 * `running` to 0, effectively ending the main loop, so that clean-up happens.
 */
void sigint_handler(int sig)
{
	running = 0;
	handled = sig;
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

	/*
	 * SUCCADE STATE
	 */

	scd_state_s state = { 0 };

	/*
	 * PARSE COMMAND LINE ARGUMENTS
	 */

	scd_prefs_s prefs = { 0 };
	state.prefs = &prefs;
	parse_args(argc, argv, state.prefs);

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
		fprintf(stderr, "DISPLAY environment variable not set, aborting.\n");
		return EXIT_FAILURE;
	}
	if (!strstr(display, ":"))
	{
		fprintf(stderr, "DISPLAY environment variable invalid, aborting.\n");
		return EXIT_FAILURE;
	}

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
	init_bar(&lemonbar);
	
	// Add references to the bar and blocks structs to the config struct
	state.lemon = &lemonbar;

	if (load_config(&state) == -1)
	{
		fprintf(stderr, "Failed to load config file: %s\n", prefs.config);
		return EXIT_FAILURE;
	}

	// If no `bin` option was present in the config, set it to the default
	if (lemonbar.bin == NULL)
	{
		lemonbar.bin = strdup(DEFAULT_LEMON_BIN);
	}

	// If no `name` option was present in the config, set it to the default
	if (lemonbar.name == NULL)
	{
		lemonbar.name = strdup(DEFAULT_LEMON_NAME);
	}

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
	 * BAR SPARK - fires when lemonbar spits something to stdout/stderr
	 */
	
	scd_spark_s bartrig = { 0 };
	bartrig.fd = lemonbar.fd_out;
	bartrig.lemon = &lemonbar;
	
	/*
	 * BLOCK SPARKS - fire when their respective commands produce output
	 */

	size_t num_triggers = create_sparks(&state);
	scd_spark_s *triggers = state.sparks;

	// Debug-print all triggers that we found
	if (DEBUG)
	{
		fprintf(stderr, "Sparks found: (%zu total)\n\t", state.num_sparks);
		for (size_t i = 0; i < state.num_sparks; ++i)
		{
			fprintf(stderr, "'%s' ", triggers[i].cmd);
		}
		fprintf(stderr, "\n");
	}

	size_t num_triggers_opened = open_triggers(triggers, state.num_sparks);
	if (DEBUG) { fprintf(stderr, "Triggered opened: %zu)\n", num_triggers_opened); }

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
				run_spark(&triggers[i]);
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
			if (process_action(&state, bar_output) < 0)
			{
				// It wasn't a recognized command, so chances are
				// that is was some debug/error output of bar.
				// TODO just use stderr in addition to stdout
				fprintf(stderr, "Lemonbar: %s", bar_output);
			}
			bar_output[0] = '\0';
		}

		// Let's update bar! TODO hardcoded value (tolerance = 0.01)
		feed_bar(&state, delta, 0.1, &wait);
	}

	/*
	 * CLEAN UP
	 */

	fprintf(stderr, "Performing clean-up ...\n");
	close(epfd);

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

