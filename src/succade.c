#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <string.h>    // strlen(), strcmp(), ...
#include <signal.h>    // sigaction(), ... 
#include <float.h>     // DBL_MAX
#include <sys/epoll.h> // epoll_wait(), ... 
#include <sys/types.h> // pid_t
#include <sys/wait.h>  // waitpid()
#include <errno.h>     // errno
#include "ini.h"       // https://github.com/benhoyt/inih
#include "succade.h"   // defines, structs, all that stuff
#include "options.c"   // Command line args/options parsing
#include "helpers.c"   // Helper functions, mostly for strings
#include "execute.c"   // Execute child processes
#include "loadini.c"   // Handles loading/processing of INI cfg file

static volatile int running;   // Used to stop main loop in case of SIGINT
static volatile int handled;   // The last signal that has been handled 
static volatile int sigchld;   // SIGCHLD has been received, please handle

/*
 * Frees all members of the given bar that need freeing.
 */
static void free_lemon(lemon_s *lemon)
{
	// TODO implement!
}

/*
 * Frees all members of the given block that need freeing.
 */
static void free_block(block_s *block)
{
	// TODO implement!
}

/*
 * Command line options and arguments string for lemonbar.
 * Allocated with malloc(), so please free() it at some point.
 */
char *lemon_arg(lemon_s *lemon)
{
	lemon_cfg_s *lcfg = &lemon->lemon_cfg;
	block_cfg_s *bcfg = &lemon->block_cfg;

	char w[8]; // TODO hardcoded (8 is what we want tho) 
	char h[8];

	snprintf(w, 8, "%d", lcfg->w);
	snprintf(h, 8, "%d", lcfg->h);

	char *block_font = optstr('f', lcfg->block_font, 0);
	char *label_font = optstr('f', lcfg->label_font, 0);
	char *affix_font = optstr('f', lcfg->affix_font, 0);
	char *name_str   = optstr('n', lcfg->name, 0);

	char *arg = malloc(sizeof(char) * BUFFER_LEMON_ARG); 

	snprintf(arg, 1024,
		"-g %sx%s+%d+%d -F%s -B%s -U%s -u%d %s %s %s %s %s %s",
		(lcfg->w > 0) ? w : "",                      // max 8
		(lcfg->h > 0) ? h : "",                      // max 8
		lcfg->x,                                     // max 8
		lcfg->y,                                     // max 8
		(bcfg->fg && bcfg->fg[0]) ? bcfg->fg : "-",  // strlen, max 9
		(lcfg->bg && lcfg->bg[0]) ? lcfg->bg : "-",  // strlen, max 9
		(bcfg->lc && bcfg->lc[0]) ? bcfg->lc : "-",  // strlen, max 9
		lcfg->lw,                                    // max 4
		(lcfg->bottom) ? "-b" : "",                  // max 2
		(lcfg->force)  ? "-d" : "",                  // max 2
		block_font,                                  // strlen, max 255
		label_font,                                  // strlen, max 255
		affix_font,                                  // strlen, max 255
		name_str                                     // strlen
	);

	free(block_font);
	free(label_font);
	free(affix_font);
	free(name_str);

	return arg;
}

/*
 * Runs the bar process and opens file descriptors for reading and writing.
 * Returns 0 on success, -1 if bar could not be started.
 */
int open_lemon(lemon_s *lemon)
{
	char *old_arg = kita_child_get_arg(lemon->child);
	free(old_arg);

	kita_child_set_arg(lemon->child, lemon_arg(lemon));
	return kita_child_open(lemon->child);
}

/*
 *
 */
int open_block(block_s *block)
{
	// TODO should we clear out child->arg here?

	if (kita_child_open(block->child) == 0)
	{
		block->last_open = get_time();
		return 0;
	}
	return -1;
}

/*
 * Send a kill signal to the lemon's child process.
 */
void close_lemon(lemon_s *lemon)
{
	kita_child_term(lemon->child);
}

/*
 * Send a kill signal to the block's child process.
 */
void close_block(block_s *block)
{
	kita_child_term(block->child);
}

/*
 * Send a kill signal to the spark's child process, then close its streams.
 */
void close_spark(spark_s *spark)
{
	kita_child_term(spark->child);
}

/*
 * Convenience function: simply runs close_block() for all blocks.
 */
void close_blocks(state_s *state)
{
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		close_block(&state->blocks[i]);
	}
}

int open_spark(spark_s *spark)
{
	return kita_child_open(spark->child);
}

/*
 * Convenience function: simply opens all given triggers.
 * Returns the number of successfully opened triggers.
 */ 
size_t open_sparks(state_s *state)
{
	size_t num_sparks_opened = 0;
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		num_sparks_opened += (open_spark(&state->sparks[i]) == 0);
	}
	return num_sparks_opened;
}

/*
 * Convenience function: simply closes all given triggers.
 */
void close_sparks(state_s *state)
{
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		close_spark(&state->sparks[i]);
	}
}

void free_spark(spark_s *t)
{
	// TODO
}

/*
 * Convenience function: simply frees all given blocks.
 */
void free_blocks(state_s *state)
{
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		free_block(&state->blocks[i]);
	}
}

/*
 * Convenience function: simply frees all given triggers.
 */
void free_sparks(state_s *state)
{
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		free_spark(&state->sparks[i]);
	}
}

int block_can_consume(block_s *block)
{
	return block->type == BLOCK_SPARKED
		&& block->block_cfg.consume
		&& !empty(block->spark->output);
}

double block_due_in(block_s *block, double now)
{
	return block->type == BLOCK_TIMED ? 
		block->block_cfg.reload - (now - block->last_open) :
		DBL_MAX;
}

int block_is_due(block_s *block, double now, double tolerance)
{
	if (block == NULL)
	{
		fprintf(stderr, "block_due(): block == NULL\n");
		return 0;
	}

	// One-shot blocks are due if they have never been run before
	if (block->type == BLOCK_ONCE)
	{
		return block->last_open == 0.0;
	}

	// Timed blocks are due if their reload time has elapsed
	if (block->type == BLOCK_TIMED)
	{
		double due_in = block_due_in(block, now);
		return due_in < tolerance;
	}

	// Sparked blocks are due if their spark has new output, or if 
	// they don't consume output and have never been run
	if (block->type == BLOCK_SPARKED)
	{
		// TODO - currently, the output is never cleared I think?
		//      - or should we check for the block's input instead?
		if (block->spark == NULL)
		{
			fprintf(stderr, "block_due(): spark missing for block '%s'\n", block->sid);
			return 0;
		}
		return block->spark->output != NULL;
	}

	// Live blocks are due if they have unread data available
	if (block->type == BLOCK_LIVE)
	{	
		return block->last_read == 0.0;
	}

	// Unknown block type (WTF?)
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
char *blockstr(const lemon_s *bar, const block_s *block, size_t len)
{
	char action_start[(5 * strlen(block->sid)) + 56]; // ... + (5 * 11) + 1
	action_start[0] = 0;
	char action_end[21]; // (5 * 4) + 1
	action_end[0] = 0;

	if (block->click_cfg.lmb)
	{
		strcat(action_start, "%{A1:");
		strcat(action_start, block->sid);
		strcat(action_start, "_lmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->click_cfg.mmb)
	{
		strcat(action_start, "%{A2:");
		strcat(action_start, block->sid);
		strcat(action_start, "_mmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->click_cfg.rmb)
	{
		strcat(action_start, "%{A3:");
		strcat(action_start, block->sid);
		strcat(action_start, "_rmb:}");
		strcat(action_end, "%{A}");
	}
	if (block->click_cfg.sup)
	{
		strcat(action_start, "%{A4:");
		strcat(action_start, block->sid);
		strcat(action_start, "_sup:}");
		strcat(action_end, "%{A}");
	}
	if (block->click_cfg.sdn)
	{
		strcat(action_start, "%{A5:");
		strcat(action_start, block->sid);
		strcat(action_start, "_sdn:}");
		strcat(action_end, "%{A}");
	}

	size_t diff;
	char *result = escape(block->output, '%', &diff);
	int padding = block->block_cfg.width + diff;

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
		buf_len += bar->block_cfg.prefix ? strlen(bar->block_cfg.prefix) : 0;
		buf_len += bar->block_cfg.suffix ? strlen(bar->block_cfg.suffix) : 0;
		buf_len += block->block_cfg.label ? strlen(block->block_cfg.label) : 0;
		buf_len += strlen(result);
	}

	const char *fg = strsel(block->block_cfg.fg, NULL, NULL);
	const char *bg = strsel(block->block_cfg.bg, bar->block_cfg.bg, NULL);
	const char *lc = strsel(block->block_cfg.lc, NULL, NULL);
	const char *label_fg = strsel(block->block_cfg.label_fg, bar->block_cfg.label_fg, fg);
	const char *label_bg = strsel(block->block_cfg.label_bg, bar->block_cfg.label_bg, bg);
	const char *affix_fg = strsel(block->block_cfg.affix_fg, bar->block_cfg.affix_fg, fg);
	const char *affix_bg = strsel(block->block_cfg.affix_bg, bar->block_cfg.affix_bg, bg);
        const int offset = (block->block_cfg.offset >= 0) ? block->block_cfg.offset : bar->block_cfg.offset;	
	const int ol = block->block_cfg.ol ? 1 : (bar->block_cfg.ol ? 1 : 0);
	const int ul = block->block_cfg.ul ? 1 : (bar->block_cfg.ul ? 1 : 0);

	// TODO somewhere I've seen someone use a combined syntax, something
	//      like this, for example: '%{T- F- B-}' instead of having them 
	//      all seperate like we do: '%{T-}%{F-}%{B-}'; let's test that!
	//      If that actually works, then we can save some bytes here. :)

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
		bar->block_cfg.prefix ? bar->block_cfg.prefix : "",    // strlen
		// Label
		label_fg ? label_fg : "-",                        // strlen, max 9
		label_bg ? label_bg : "-",                        // strlen, max 9
		block->block_cfg.label ? block->block_cfg.label : "",  // strlen
		// Block
		fg ? fg : "-",                                    // strlen, max 9
		bg ? bg : "-",                                    // strlen, max 9
		padding,                                          // max 4
		result,                                           // strlen
		// Suffix
		affix_fg ? affix_fg : "-",                        // strlen, max 9
		affix_bg ? affix_bg : "-",                        // strlen, max 9
		bar->block_cfg.suffix ? bar->block_cfg.suffix : "",    // strlen
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
char *barstr(const state_s *state)
{
	// For convenience...
	const lemon_s *bar = &state->lemon;
	const block_s *blocks = state->blocks;
	size_t num_blocks = state->num_blocks;

	// Short blocks like temperature, volume or battery, will usually use 
	// something in the range of 130 to 200 byte. So let's go with 256 byte.
	size_t bar_str_len = 256 * num_blocks; // TODO hardcoded value
	char *bar_str = malloc(bar_str_len);
	bar_str[0] = '\0';

	char align[5];
	int last_align = -1;

	const block_s *block = NULL;
	for (int i = 0; i < num_blocks; ++i)
	{
		block = &blocks[i];

		// Live blocks might not have a result available
		if (block->output == NULL)
		{
			continue;
		}

		char *block_str = blockstr(bar, block, 0);
		size_t block_str_len = strlen(block_str);
		if (block->block_cfg.align != last_align)
		{
			last_align = block->block_cfg.align;
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
 * Parses the format string for the bar, which should contain block names 
 * separated by whitespace and, optionally, up to two vertical bars to indicate 
 * alignment of blocks. For every block name found, the callback function `cb` 
 * will be run. Returns the number of block names found.
 */
size_t parse_format(const char *format, create_block_callback cb, void *data)
{
	if (format == NULL)
	{
		return 0;
	}

	size_t format_len = strlen(format) + 1;
	char block_name[BUFFER_BLOCK_NAME];
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

	// Return the number of blocks found
	return num_blocks;
}

kita_child_s* make_child(state_s *state, const char *cmd, int in, int out, int err)
{
	// Create child process
	kita_child_s *child = kita_child_new(cmd, 1, 1, 1);
	if (child == NULL)
	{
		return NULL;
	}

	// Add the child to the kita 
	if (kita_child_add(state->kita, child) == -1)
	{
		kita_child_free(&child);
		return NULL;
	}

	return child;
}

/*
 * Finds and returns the block with the given `sid` -- or NULL.
 */
block_s *get_block(const state_s *state, const char *sid)
{
	// Iterate over all existing blocks and check for a name match
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		// If names match, return a pointer to this block
		if (equals(state->blocks[i].sid, sid))
		{
			return &state->blocks[i];
		}
	}
	return NULL;
}

/*
 * Add the block with the given SID to the collection of blocks, unless there 
 * is already a block with that SID present. 
 * Returns a pointer to the added (or existing) block or NULL in case of error.
 */
block_s *add_block(state_s *state, const char *sid)
{
	// See if there is an existing block by this name (and return, if so)
	block_s *eb = get_block(state, sid);
	if (eb)
	{
		return eb;
	}

	// Resize the block container to be able to hold one more block
	size_t current  =   state->num_blocks;
	size_t new_size = ++state->num_blocks * sizeof(block_s);
	block_s *blocks = realloc(state->blocks, new_size);
	if (blocks == NULL)
	{
		fprintf(stderr, "add_block(): realloc() failed!\n");
		--state->num_blocks;
		return NULL;
	}
	state->blocks = blocks;

	// Create the block, setting its name and default values
	state->blocks[current] = (block_s) { 0 };
	state->blocks[current].sid = strdup(sid);

	// Return a pointer to the new block
	return &state->blocks[current];
}

/*
 * inih doc: "Handler should return nonzero on success, zero on error."
 */
int lemon_cfg_handler(void *data, const char *section, const char *name, const char *value)
{
	state_s *state = (state_s*) data;

	// Only process if section is empty or specificially for bar
	if (empty(section) || equals(section, state->lemon.sid))
	{
		return lemon_ini_handler(&state->lemon, section, name, value);
	}

	return 1;
}

/*
 * inih doc: "Handler should return nonzero on success, zero on error."
 */
int block_cfg_handler(void *data, const char *section, const char *name, const char *value)
{
	state_s *state = (state_s*) data;

	// Abort if section is empty or specifically for bar
	if (empty(section) || (equals(section, state->lemon.sid)))
	{
		return 1;
	}

	// Find the block whose name fits the section name
	block_s *block = get_block(state, section);

	// Abort if we couldn't find that block
	if (block == NULL)
	{
		return 1;
	}

	// Process via the appropriate handler
	return block_ini_handler(block, section, name, value);
}

/*
 * Load the config and parse the section for the bar, ignoring other sections.
 * Returns 0 on success, -1 on file open error, -2 on memory allocation error, 
 * -3 if no config file path was given in the preferences, or the line number 
 * of the first encountered parse error.
 */
static int load_lemon_cfg(state_s *state)
{
	// Abort if config file path empty or NULL
	if (empty(state->prefs.config))
	{
		return -3;
	}

	// Fire up the INI parser
	return ini_parse(state->prefs.config, lemon_cfg_handler, state);
}

/*
 * Load the config and parse all sections apart from the bar section.
 * Returns 0 on success, -1 on file open error, -2 on memory allocation error, 
 * -3 if no config file path was given in the preferences, or the line number 
 * of the first encountered parse error.
 */
static int load_block_cfg(state_s *state)
{
	// Abort if config file path empty or NULL
	if (empty(state->prefs.config))
	{
		return -3;
	}

	return ini_parse(state->prefs.config, block_cfg_handler, state);
}

spark_s *get_spark(state_s *state, void *block, const char *cmd)
{
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		if (state->sparks[i].block != block)
		{
			continue;
		}
		if (!equals(state->sparks[i].child->cmd, cmd))
		{
			continue;
		}
		return &state->sparks[i];
	}
	return NULL;
}

spark_s *add_spark(state_s *state, block_s *block, const char *cmd)
{
	// See if there is an existing spark that matches the given params
	spark_s *es = get_spark(state, block, cmd);
	if (es)
	{
		return es;
	}

	// Resize the spark array to be able to hold one more spark
	size_t current  =   state->num_sparks;
	size_t new_size = ++state->num_sparks * sizeof(spark_s);
	spark_s *sparks = realloc(state->sparks, new_size);
	if (sparks == NULL)
	{
		fprintf(stderr, "add_spark(): realloc() failed!\n");
		--state->num_sparks;
		return NULL;
	}
	state->sparks = sparks; 
	 
	state->sparks[current] = (spark_s) { 0 };
	state->sparks[current].block = block;

	// Add a reference of this spark to the block we've created it for
	block->spark = &state->sparks[current];

	// Return a pointer to the new spark
	return &state->sparks[current];
}

size_t create_sparks(state_s *state)
{
	block_s *block = NULL;
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		block = &state->blocks[i];

		if (block->type != BLOCK_SPARKED)
		{
			continue;
		}

		if (empty(block->block_cfg.trigger))
		{
			fprintf(stderr, "create_sparks(): missing trigger for sparked block '%s'\n", block->sid);
			continue;
		}

		add_spark(state, block, block->block_cfg.trigger);
	}

	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		state->sparks[i].child = kita_child_new(state->sparks[i].block->block_cfg.trigger, 0, 1, 0);
		kita_child_add(state->kita, state->sparks[i].child);
	}

	return state->num_sparks;
}

/*
 * Takes a string that might represent an action that was registered with one 
 * of the blocks and tries to find the associated block. If found, the command
 * associated with the action will be executed.
 * Returns 0 on success, -1 if the string was not a recognized action command
 * or the block that the action belongs to could not be found.
 */
int process_action(const state_s *state, const char *action)
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
	block_s *source = get_block(state, block);
	if (source == NULL)
	{
		return -1;
	}

	// Now to fire the right command for the action type
	switch (b) {
		case 0:
			run_cmd(source->click_cfg.lmb);
			return 0;
		case 1:
			run_cmd(source->click_cfg.mmb);
			return 0;
		case 2:
			run_cmd(source->click_cfg.rmb);
			return 0;
		case 3:
			run_cmd(source->click_cfg.sup);
			return 0;
		case 4:
			run_cmd(source->click_cfg.sdn);
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
static void on_block_found(const char *name, int align, int n, void *data)
{
	// 'Unpack' the data
	state_s *state = (state_s*) data;
	
	// Find or add the block with the given name
	block_s *block = add_block(state, name);
	
	if (block == NULL)
	{
		fprintf(stderr, "on_block_found(): add_block() failed!\n");
		return;
	}
	// Set the block's align to the given one
	block->block_cfg.align = align;
}

/*
 * Handles SIGINT signals (CTRL+C) by setting the static variable
 * `running` to 0, effectively ending the main loop, so that clean-up happens.
 */
void on_sigint(int sig)
{
	running = 0;
	handled = sig;
}

void on_child_readok(kita_state_s *ks, kita_event_s *ke)
{
	char *output = kita_child_read(ke->child, ke->ios);
	fprintf(stderr, "on_child_readok(): %s\n", output);
	free(output);
}

void on_child_exited(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_exited)\n");
}

void on_child_closed(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_closed()\n");
}

void on_child_reaped(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_reaped()\n");
}

void on_child_remove(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_remove()\n");
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
	fprintf(stderr, "\t-s\n");
	fprintf(stderr, "\t\tINI section name for the bar.\n");
}

int main(int argc, char **argv)
{
	/*
	 * SIGNALS
	 */

	// Make sure we still do clean-up on SIGINT (ctrl+c)
	// and similar signals that indicate we should quit.
	struct sigaction sa_int = {
		.sa_handler = &on_sigint
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
	 * SUCCADE STATE
	 */

	state_s  state = { 0 };
	prefs_s *prefs = &(state.prefs); // For convenience
	lemon_s *lemon = &(state.lemon); // For convenience

	/*
	 * KITA STATE
	 */

	state.kita = kita_init();
	if (state.kita == NULL)
	{
		fprintf(stderr, "Failed to initialize kita state, aborting.\n");
		return EXIT_FAILURE;
	}
	kita_state_s *kita = state.kita; // For convenience

	/* 
	 * REGISTER CALLBACKS 
	 */

	kita_set_callback(kita, KITA_EVT_CHILD_CLOSED, on_child_closed);
	kita_set_callback(kita, KITA_EVT_CHILD_REAPED, on_child_reaped);
	kita_set_callback(kita, KITA_EVT_CHILD_HANGUP, on_child_exited);
	kita_set_callback(kita, KITA_EVT_CHILD_EXITED, on_child_exited);
	kita_set_callback(kita, KITA_EVT_CHILD_REMOVE, on_child_remove);
	kita_set_callback(kita, KITA_EVT_CHILD_READOK, on_child_readok);


	/*
	 * COMMAND LINE ARGUMENTS
	 */

	parse_args(argc, argv, prefs);
	char *default_cfg_path = NULL;

	/*
	 * PRINT HELP AND EXIT, MAYBE
	 */

	if (prefs->help)
	{
		help(argv[0]);
		return EXIT_SUCCESS;
	}

	/*
	 * PREFERENCES / DEFAULTS
	 */

	// If no custom config file given, set it to the default
	if (prefs->config == NULL)
	{
		// We use the additional variable for consistency with free()
		default_cfg_path = config_path(DEFAULT_CFG_FILE, SUCCADE_NAME);
		prefs->config = default_cfg_path; 
	}

	// If no custom INI section for bar given, set it to default
	if (prefs->section == NULL)
	{
		prefs->section = DEFAULT_LEMON_SECTION;
	}

	/*
	 * BAR
	 */

	// Copy the Section ID from the config for convenience and consistency
	lemon->sid = strdup(prefs->section);

	// Read the config file and parse bar's section
	if (load_lemon_cfg(&state) < 0)
	{
		fprintf(stderr, "Failed to load config file: %s\n", prefs->config);
		return EXIT_FAILURE;
	}
	// If no `bin` option was present in the config, set it to the default
	if (empty(lemon->lemon_cfg.bin))
	{
		// We use strdup() for consistency with free() later on
		lemon->lemon_cfg.bin = strdup(DEFAULT_LEMON_BIN);
	}

	// If no `name` option was present in the config, set it to the default
	if (empty(lemon->lemon_cfg.name))
	{
		// We use strdup() for consistency with free() later on
		lemon->lemon_cfg.name = strdup(DEFAULT_LEMON_NAME);
	}

	// Create the child process and add it to the kita state
	lemon->child = make_child(&state, lemon->lemon_cfg.bin, 1, 1, 1);
	if (lemon->child == NULL)
	{
		fprintf(stderr, "Failed to create lemon process: %s\n", lemon->lemon_cfg.bin);
		return EXIT_FAILURE;
	}

	// Open (run) the lemon
	if (open_lemon(lemon) == -1)
	{
		fprintf(stderr, "Failed to open bar: %s\n", lemon->sid);
		return EXIT_FAILURE;
	}

	/*
	 * BLOCKS
	 */

	// Create blocks by parsing the format string
	size_t parsed = parse_format(lemon->lemon_cfg.format, on_block_found, &state);

	fprintf(stderr, "Number of blocks: parsed = %zu, configured = %zu\n", 
			parsed, state.num_blocks);

	// Exit if no blocks could be loaded and 'empty' option isn't present
	if (state.num_blocks == 0 && prefs->empty == 0)
	{
		fprintf(stderr, "No blocks loaded, stopping %s.\n", SUCCADE_NAME);
		return EXIT_FAILURE;
	}

	// Parse the config again, this time processing block sections
	if (load_block_cfg(&state) < 0)
	{
		fprintf(stderr, "Failed to load config file: %s\n", prefs->config);
		return EXIT_FAILURE;
	}

	if (DEBUG)
	{
		for (size_t i = 0; i < state.num_blocks; ++i)
		{
			fprintf(stderr, "Block #%zu: %s -> %s\n", i, 
					state.blocks[i].sid,
					state.blocks[i].block_cfg.bin);
		}
	}

	// Create child processes and add them to the kita state
	block_s *block = NULL;
	for (size_t i = 0; i < state.num_blocks; ++i)
	{
		block = &state.blocks[i];
		char *block_cmd = block->block_cfg.bin ? block->block_cfg.bin : block->sid;
		block->child = make_child(&state, block_cmd, 0, 1, 1);
	}

	/*
	// Run all LIVE blocks
	for (int i = 0; i < state.num_blocks; ++i)
	{
		if (state.blocks[i].type == BLOCK_LIVE)
		{
			open_block(&state.blocks[i]);
		}
	}
	*/

	/*
	 * SPARKS
	 */

	create_sparks(&state);
	
	/*
	size_t num_sparks_opened = open_sparks(&state);
	
	if (DEBUG)
	{
		fprintf(stderr, "Number of sparks: parsed = %zu, opened = %zu\n", 
				state.num_sparks, num_sparks_opened);
	}
	*/

	/*
	 * MAIN LOOP
	 */

	double now;
	double before = get_time();
	double delta;
	double wait = 0.0; // Will later be set to suitable value by feed_lemon()

	//char bar_output[BUFFER_SIZE];
	//bar_output[0] = '\0';

	running = 1;
	
	while (running)
	{
		now    = get_time();
		delta  = now - before;
		before = now;

		fprintf(stderr, "> wait = %f, delta = %f\n", wait, delta);

		// start all blocks that haven't been run yet (exception: sparked blocks)
		block_s *block = NULL;
		for (size_t i = 0; i < state.num_blocks; ++i)
		{
			block = &state.blocks[i];
			if (block->last_open == 0.0 && block->type != BLOCK_SPARKED)
			{
				kita_child_open(block->child);
				block->last_open = get_time();
			}
		}

		// start all sparks that haven't been run yet
		spark_s *spark = NULL;
		for (size_t i = 0; i < state.num_sparks; ++i)
		{
			spark = &state.sparks[i];
			if (spark->last_open == 0.0)
			{
				kita_child_open(spark->child);
				spark->last_open = get_time();
			}
		}

		//kita_tick(kita, 1000);
		kita_tick(kita, wait * 1000);

		// Figure out when we need to run next (timed blocks)
		double lemon_due = DBL_MAX;
		double thing_due = DBL_MAX;
		for (size_t i = 0; i < state.num_blocks; ++i)
		{
			block = &state.blocks[i];
			thing_due = block_due_in(block, now);

			if (thing_due < lemon_due)
			{
				lemon_due = thing_due;
			}
		}

		// Update `wait` accordingly (-1 = not waiting on any blocks)
		wait = lemon_due == DBL_MAX ? -1 : lemon_due;

		// TODO - feed lemonbar
		//      - figure out time till next tick



		/*
		// Figure out when we need to run next (timed blocks)
		double lemon_due = DBL_MAX;
		double thing_due = DBL_MAX;
		for (size_t i = 0; i < state.num_blocks; ++i)
		{
			block = &state.blocks[i];
			thing_due = block_due_in(block, now);

			if (thing_due < lemon_due)
			{
				lemon_due = thing_due;
			}
		}

		// Update `wait` accordingly (-1 = not waiting on any blocks)
		wait = lemon_due == DBL_MAX ? -1 : lemon_due;

		// Let's see if Lemonbar produced any output
		if (bartrig.ready)
		{
			fgets(bar_output, BUFFER_SIZE, lemonbar.fp[FD_OUT]);
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

		// Let's update bar! 
		char *lemon_input = barstr(&state);
		feed_child(&lemon->child, lemon_input);
		free(lemon_input);
		*/
	}

	/*
	 * CLEAN UP
	 */

	fprintf(stderr, "Performing clean-up ...\n");

	free(default_cfg_path);

	// Close triggers - it's important we free these first as they might
	// point to instances of bar and/or blocks, which will lead to errors
	close_sparks(&state);
	free_sparks(&state);
	free(state.sparks);
	
	/*
	close_spark(&bartrig);
	free_spark(&bartrig);
	*/

	// Close blocks
	close_blocks(&state);
	free_blocks(&state);
	free(state.blocks);

	// Close bar
	close_lemon(&state.lemon);
	free_lemon(&state.lemon);

	fprintf(stderr, "Clean-up finished, see you next time!\n");

	return EXIT_SUCCESS;
}

