#define CFG_IMPLEMENTATION
#define KITA_IMPLEMENTATION

#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <string.h>    // strlen(), strcmp(), ...
#include <signal.h>    // sigaction(), ... 
#include <float.h>     // DBL_MAX
#include "ini.h"       // https://github.com/benhoyt/inih
#include "cfg.h"
#include "libkita.h"
#include "succade.h"   // defines, structs, all that stuff
#include "options.c"   // Command line args/options parsing
#include "helpers.c"   // Helper functions, mostly for strings
#include "loadini.c"   // Handles loading/processing of INI cfg file

static volatile int running;   // used to stop main loop 
static volatile int handled;   // last signal that has been handled 

/*
 * Frees all members of the given thing that need freeing.
 */
static void free_thing(thing_s *thing)
{
	free(thing->sid);
	free(thing->output);
	cfg_free(&thing->cfg);

	char *arg = kita_child_get_arg(thing->child);
	free(arg);
}

/*
 * Command line options and arguments string for lemonbar.
 * Allocated with malloc(), so please free() it at some point.
 */
static char *lemon_arg(thing_s *lemon)
{
	cfg_s *lcfg = &lemon->cfg;

	char w[BUFFER_NUMERIC]; 
	char h[BUFFER_NUMERIC];

	snprintf(w, BUFFER_NUMERIC, "%d", cfg_get_int(lcfg, LEMON_OPT_WIDTH));
	snprintf(h, BUFFER_NUMERIC, "%d", cfg_get_int(lcfg, LEMON_OPT_HEIGHT));

	char *block_font = optstr('f', cfg_get_str(lcfg, LEMON_OPT_BLOCK_FONT), 0);
	char *label_font = optstr('f', cfg_get_str(lcfg, LEMON_OPT_LABEL_FONT), 0);
	char *affix_font = optstr('f', cfg_get_str(lcfg, LEMON_OPT_AFFIX_FONT), 0);
	char *name_str   = optstr('n', cfg_get_str(lcfg, LEMON_OPT_NAME), 0);

	char *fg = cfg_get_str(lcfg, LEMON_OPT_FG);
	char *bg = cfg_get_str(lcfg, LEMON_OPT_BG);
	char *lc = cfg_get_str(lcfg, LEMON_OPT_LC);

	char *arg = malloc(BUFFER_LEMON_ARG); 

	snprintf(arg, BUFFER_LEMON_ARG,
		"-g %sx%s+%d+%d -a%d -F%s -B%s -U%s -u%d %s %s %s %s %s %s",
		cfg_has(lcfg, LEMON_OPT_WIDTH)  ? w : "",    // max 8
		cfg_has(lcfg, LEMON_OPT_HEIGHT) ? h : "",    // max 8
		cfg_get_int(lcfg, LEMON_OPT_X),              // max 8
		cfg_get_int(lcfg, LEMON_OPT_Y),              // max 8
		cfg_get_int(lcfg, LEMON_OPT_AREAS),          // 
		fg ? fg : "-",                               // strlen, max 9
		bg ? bg : "-",                               // strlen, max 9
		lc ? lc : "-",                               // strlen, max 9
		cfg_get_int(lcfg, LEMON_OPT_LW),             // max 4
		cfg_get_int(lcfg, LEMON_OPT_BOTTOM) ? "-b" : "",   // max 2
		cfg_get_int(lcfg, LEMON_OPT_FORCE)  ? "-d" : "",   // max 2
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
 * Runs the lemon's child process. Returns 0 on success, -1 on error.
 */
static int open_lemon(thing_s *lemon)
{
	char *old_arg = kita_child_get_arg(lemon->child);
	free(old_arg);

	kita_child_set_arg(lemon->child, lemon_arg(lemon));
	if (kita_child_open(lemon->child) == 0)
	{
		return kita_child_set_buf_type(lemon->child, KITA_IOS_IN, KITA_BUF_LINE);
	}

	return -1;
}

/*
 * Runs the block's child process. Returns 0 on success, -1 on error.
 */
static int open_block(thing_s *block)
{
	if (kita_child_open(block->child) == 0)
	{
		block->last_open = get_time();
		block->alive = 1;
		return 0;
	}
	return -1;
}

static int open_spark(thing_s *spark)
{
	if (kita_child_open(spark->child) == 0)
	{
		spark->last_open = get_time();
		spark->alive = 1;
		return 0;
	}
	return -1;
}

/*
 * Read from the block's stdout and save the read data, if any, in the block's 
 * output field. Returns 0 if the read data was the same as the previous data
 * already present in the output field, 1 if the newly read data is different.
 */
static int read_block(thing_s *block)
{
	char *old = block->output ? strdup(block->output) : NULL;

	free(block->output); // just in case, free'ing NULL is fine
	block->output = kita_child_read(block->child, KITA_IOS_OUT);
	block->last_read = get_time();
	
	int same = (old && equals(old, block->output));
	free(old);

	return !same;
}

/*
 * Read from the spark's stdout and save the read data, if any, in the spark's
 * output field. Returns 0 if no data (or an empty string) was read, else 1.
 */
static int read_spark(thing_s *spark)
{
	free(spark->output); // just in case, free'ing NULL is fine
	spark->output = kita_child_read(spark->child, KITA_IOS_OUT);
	spark->last_read = get_time();

	return !empty(spark->output);
}

/*
 * Convenience function: simply opens all given triggers.
 * Returns the number of successfully opened triggers.
 */ 
static size_t open_sparks(state_s *state)
{
	size_t num_sparks_opened = 0;
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		num_sparks_opened += (open_spark(&state->sparks[i]) == 0);
	}
	return num_sparks_opened;
}

/*
 * Convenience function: simply frees all given blocks.
 */
static void free_blocks(state_s *state)
{
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		free_thing(&state->blocks[i]);
	}
}

/*
 * Convenience function: simply frees all given triggers.
 */
static void free_sparks(state_s *state)
{
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		free_thing(&state->sparks[i]);
	}
}

static int block_can_consume(thing_s *block)
{
	return block->b_type == BLOCK_SPARKED
		&& cfg_get_int(&block->cfg, BLOCK_OPT_CONSUME) 
		&& !empty(block->other->output);
}

static double block_due_in(thing_s *block, double now)
{
	float reload = cfg_get_float(&block->cfg, BLOCK_OPT_RELOAD);

	return block->b_type == BLOCK_TIMED ? 
		reload - (now - block->last_open) : 
		DBL_MAX;
}

static int block_is_due(thing_s *block, double now, double tolerance)
{
	// block is currently running
	if (block->alive)
	{
		return 0;
	}

	// One-shot blocks are due if they have never been run before
	if (block->b_type == BLOCK_ONCE)
	{
		return block->last_open == 0.0;
	}

	// Timed blocks are due if their reload time has elapsed
	// or if they've never been run before
	if (block->b_type == BLOCK_TIMED)
	{
		if (block->last_open == 0.0)
		{
			return 1;
		}
		double due_in = block_due_in(block, now);
		return due_in < tolerance;
	}

	// Sparked blocks are due if their spark has new output, or if 
	// they don't consume output and have never been run
	if (block->b_type == BLOCK_SPARKED)
	{
		// spark missing
		if (block->other == NULL)
		{
			return 0;
		}

		// spark has output waiting to be processed
		if (block->other->output)
		{
			return 1;
		}

		// doesn't consume and has never been run before
		if (cfg_get_int(&block->cfg, BLOCK_OPT_CONSUME) == 0)
		{
			return block->last_open == 0.0;
		}
	}

	// Live blocks are due if they haven't been run yet 
	if (block->b_type == BLOCK_LIVE)
	{	
		return block->last_open == 0.0;
	}

	// Unknown block type (WTF?)
	return 0;
}

/*
 * Opens all blocks that are due and returns the number of blocks opened.
 */
static size_t open_due_blocks(state_s *state, double now)
{
	size_t opened = 0;
	thing_s *block = NULL;
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		block = &state->blocks[i];
		if (block_is_due(block, now, BLOCK_WAIT_TOLERANCE))
		{
			if (block_can_consume(block))
			{
				kita_child_set_arg(block->child, block->other->output);
				opened += (open_block(block) == 0);
				kita_child_set_arg(block->child, NULL);
			}
			else
			{
				opened += (open_block(block) == 0);
			}
			if (block->b_type == BLOCK_SPARKED)
			{
				free(block->other->output);
				block->other->output = NULL;
			}
		}
	}
	return opened;
}

/*
 * Returns the time, in seconds, until the next block should be run.
 */
static double time_to_wait(state_s *state, double now)
{
	double lemon_due = DBL_MAX;
	double thing_due = DBL_MAX;

	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		thing_due = block_due_in(&state->blocks[i], now);

		if (thing_due < lemon_due)
		{
			lemon_due = thing_due;
		}
	}

	return (lemon_due == DBL_MAX) ? -1 : lemon_due;
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
static char *blockstr(const thing_s *lemon, const thing_s *block)
{
	// for convenience
	const cfg_s *bcfg = &block->cfg;
	const cfg_s *lcfg = &lemon->cfg;

	char action_start[(5 * strlen(block->sid)) + 64];
	char action_end[24];  
	action_start[0] = 0;
	action_end[0]   = 0;

	if (cfg_has(bcfg, BLOCK_OPT_CMD_LMB))
	{
		strcat(action_start, "%{A1:");
		strcat(action_start, block->sid);
		strcat(action_start, "_lmb:}");
		strcat(action_end, "%{A}");
	}
	if (cfg_has(bcfg, BLOCK_OPT_CMD_MMB))
	{
		strcat(action_start, "%{A2:");
		strcat(action_start, block->sid);
		strcat(action_start, "_mmb:}");
		strcat(action_end, "%{A}");
	}
	if (cfg_has(bcfg, BLOCK_OPT_CMD_RMB))
	{
		strcat(action_start, "%{A3:");
		strcat(action_start, block->sid);
		strcat(action_start, "_rmb:}");
		strcat(action_end, "%{A}");
	}
	if (cfg_has(bcfg, BLOCK_OPT_CMD_SUP))
	{
		strcat(action_start, "%{A4:");
		strcat(action_start, block->sid);
		strcat(action_start, "_sup:}");
		strcat(action_end, "%{A}");
	}
	if (cfg_has(bcfg, BLOCK_OPT_CMD_SDN))
	{
		strcat(action_start, "%{A5:");
		strcat(action_start, block->sid);
		strcat(action_start, "_sdn:}");
		strcat(action_end, "%{A}");
	}

	// TODO we're missing the UNIT option, add that! let's have the
	//      unit have the same foreground and background color as 
	//      the actual block result string, that should be okay.
	//      but, be careful! the length of the unit string, plus the 
	//      separating space, need to be taken into account when 
	//      calculating the padding (fixed width) of the block, no?
	
	size_t diff = 0;
	char *result = (cfg_has(bcfg, BLOCK_OPT_RAW) && cfg_get_int(bcfg, BLOCK_OPT_RAW)) ? 
		strdup(block->output) : escape(block->output, '%', &diff);
	int min_width = cfg_get_int(bcfg, BLOCK_OPT_WIDTH) + diff;

	const char *block_fg = strsel(cfg_get_str(bcfg, BLOCK_OPT_FG), "-", "-");
	const char *block_bg = strsel(cfg_get_str(bcfg, BLOCK_OPT_BG), cfg_get_str(lcfg, LEMON_OPT_BLOCK_BG), "-");
	const char *label_fg = strsel(cfg_get_str(bcfg, BLOCK_OPT_LABEL_FG), cfg_get_str(lcfg, LEMON_OPT_LABEL_FG), "-");
	const char *label_bg = strsel(cfg_get_str(bcfg, BLOCK_OPT_LABEL_BG), cfg_get_str(lcfg, LEMON_OPT_LABEL_BG), "-");
	const char *affix_fg = strsel(cfg_get_str(bcfg, BLOCK_OPT_AFFIX_FG), cfg_get_str(lcfg, LEMON_OPT_AFFIX_FG), "-");
	const char *affix_bg = strsel(cfg_get_str(bcfg, BLOCK_OPT_AFFIX_BG), cfg_get_str(lcfg, LEMON_OPT_AFFIX_BG), "-");

	int padding  = cfg_get_int(lcfg, LEMON_OPT_BLOCK_PADDING);
	int margin   = cfg_get_int(lcfg, LEMON_OPT_BLOCK_MARGIN);
	int margin_l = cfg_has(bcfg, BLOCK_OPT_MARGIN_LEFT) ? cfg_get_int(bcfg, BLOCK_OPT_MARGIN_LEFT) : -1;
	int margin_r = cfg_has(bcfg, BLOCK_OPT_MARGIN_RIGHT) ? cfg_get_int(bcfg, BLOCK_OPT_MARGIN_RIGHT) : -1;
	int ol     = cfg_has(bcfg, BLOCK_OPT_OL) ? cfg_get_int(bcfg, BLOCK_OPT_OL) : cfg_get_int(lcfg, LEMON_OPT_OL);
	int ul     = cfg_has(bcfg, BLOCK_OPT_UL) ? cfg_get_int(bcfg, BLOCK_OPT_UL) : cfg_get_int(lcfg, LEMON_OPT_UL);
	const char *lc   = strsel(cfg_get_str(bcfg, BLOCK_OPT_LC), cfg_get_str(lcfg, LEMON_OPT_LC) , "-");

	const char *prefix = strsel(cfg_get_str(bcfg, BLOCK_OPT_PREFIX), cfg_get_str(lcfg, LEMON_OPT_BLOCK_PREFIX), "");
	const char *suffix = strsel(cfg_get_str(bcfg, BLOCK_OPT_PREFIX), cfg_get_str(lcfg, LEMON_OPT_BLOCK_SUFFIX), "");
	const char *label  = strsel(cfg_get_str(bcfg, BLOCK_OPT_LABEL), "", "");
	//const char *unit   = strsel(cfg_get_str(bcfg, BLOCK_OPT_UNIT), "", "");

	// TODO currently we are adding the format thingies for label, 
	//      prefix and suffix, even if those are empty anyway, which
	//      makes the string much longer than it needs to be, hence 
	//      also increasing the parsing workload for lemonbar.
	//      but of course, replacing a couple bytes with lots of malloc
	//      would not be great either, so... not sure about it yet.

	char *str = malloc(BUFFER_BLOCK_STR);
	snprintf(str, BUFFER_BLOCK_STR,
		"%%{O%d}"
		"%s%%{F%s B%s U%s %co %cu}"
		"%%{T3 F%s B%s}%s"
		"%%{T2 F%s B%s}%s"
		"%%{T1 F%s B%s}%*s%*s%*s"
		"%%{T3 F%s B%s}%s"
		"%%{T- F- B- U- -o -u}%s"
		"%%{O%d}",
		// margin left
		margin_l >= 0 ? margin_l : margin,
		// start
		action_start, block_fg, block_bg, lc, (ol ? '+' : '-'), (ul ? '+' : '-'),
		// prefix
		affix_fg, affix_bg, prefix,
		// label
		label_fg, label_bg, label,
		// block
		block_fg, block_bg, padding, "", min_width, result, padding, "",
		// suffix
		affix_fg, affix_bg, suffix,
		// end
		action_end,
		margin_r >= 0 ? margin_r : margin
	);

	free(result);
	return str;
}

/*
 * Returns 'l', 'c' or 'r' for input values -1, 0 and 1 respectively.
 * For other input values, the behavior is undefined.
 */
static char get_align(const int align)
{
	char a[] = {'l', 'c', 'r'};
	return a[align+1]; 
}

/*
 * Combines the results of all given blocks into a single string that can be fed
 * to Lemonbar. Returns a pointer to the string, allocated with malloc().
 */
static char *barstr(const state_s *state)
{
	// This should never happen, but just in case (also makes compiler happy)
	if (state->num_blocks == 0)
	{
		return NULL;
	}
	
	// For convenience...
	const thing_s *bar = &state->lemon;
	size_t num_blocks = state->num_blocks;

	// Short blocks like temperature, volume or battery, will usually use 
	// something in the range of 130 to 200 byte. So let's go with 256 byte.
	size_t bar_str_len = BUFFER_BLOCK_RESULT * num_blocks;
	char *bar_str = malloc(bar_str_len);
	bar_str[0] = '\0';

	char align[5];
	int last_align = -1;

	const thing_s *block = NULL;
	for (size_t i = 0; i < num_blocks; ++i)
	{
		block = &state->blocks[i];

		// Live blocks might not have a result available
		if (block->output == NULL)
		{
			continue;
		}

		int block_align = cfg_get_int(&block->cfg, BLOCK_OPT_ALIGN);

		char *block_str = blockstr(bar, block);
		size_t block_str_len = strlen(block_str);
		if (block_align != last_align)
		{
			last_align = block_align;
			snprintf(align, 5, "%%{%c}", get_align(last_align));
			strcat(bar_str, align);
		}
		// Let's check if this block string can fit in our buffer
		size_t free_len = bar_str_len - (strlen(bar_str) + 1);
		if (block_str_len > free_len)
		{
			// Let's make space for approx. two more blocks
			bar_str_len += BUFFER_BLOCK_RESULT * 2; 
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
static size_t parse_format(const char *format, create_block_callback cb, void *data)
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
			break;
		case ' ':
		case '\0':
			if (block_name_len)
			{
				// Block name complete, inform the callback
				cb(block_name, block_align, data);
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

static kita_child_s* make_child(state_s *state, const char *cmd, int in, int out, int err)
{
	// Create child process
	kita_child_s *child = kita_child_new(cmd, in, out, err);
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

	kita_child_set_context(child, state);
	return child;
}

/*
 * Finds and returns the block with the given `sid` -- or NULL.
 */
static thing_s *get_block(const state_s *state, const char *sid)
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
static thing_s *add_block(state_s *state, const char *sid)
{
	// See if there is an existing block by this name (and return, if so)
	thing_s *eb = get_block(state, sid);
	if (eb)
	{
		return eb;
	}

	// Resize the block container to be able to hold one more block
	size_t current  =   state->num_blocks;
	size_t new_size = ++state->num_blocks * sizeof(thing_s);
	thing_s *blocks = realloc(state->blocks, new_size);
	if (blocks == NULL)
	{
		fprintf(stderr, "add_block(): realloc() failed!\n");
		--state->num_blocks;
		return NULL;
	}
	state->blocks = blocks;

	// Create the block, setting its name and default values
	state->blocks[current] = (thing_s) { 0 };
	state->blocks[current].sid = strdup(sid);
	state->blocks[current].t_type = THING_BLOCK;
	state->blocks[current].b_type = BLOCK_ONCE;
	cfg_init(&state->blocks[current].cfg, "default", BLOCK_OPT_COUNT);

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
	thing_s *block = get_block(state, section);

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

static thing_s *get_spark(state_s *state, void *block, const char *cmd)
{
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		if (state->sparks[i].other != block)
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

static thing_s *add_spark(state_s *state, thing_s *block, const char *cmd)
{
	// See if there is an existing spark that matches the given params
	thing_s *es = get_spark(state, block, cmd);
	if (es)
	{
		return es;
	}

	// Resize the spark array to be able to hold one more spark
	size_t current  =   state->num_sparks;
	size_t new_size = ++state->num_sparks * sizeof(thing_s);
	thing_s *sparks = realloc(state->sparks, new_size);
	if (sparks == NULL)
	{
		fprintf(stderr, "add_spark(): realloc() failed!\n");
		--state->num_sparks;
		return NULL;
	}
	state->sparks = sparks; 
	 
	state->sparks[current] = (thing_s) { 0 };
	state->sparks[current].t_type = THING_SPARK;
	state->sparks[current].other = block;

	// Add a reference of this spark to the block we've created it for
	block->other = &state->sparks[current];

	// Return a pointer to the new spark
	return &state->sparks[current];
}

static size_t create_sparks(state_s *state)
{
	thing_s *block = NULL;
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		block = &state->blocks[i];

		if (block->b_type != BLOCK_SPARKED)
		{
			continue;
		}

		char *trigger = cfg_get_str(&block->cfg, BLOCK_OPT_TRIGGER);
		if (empty(trigger))
		{
			fprintf(stderr, "create_sparks(): missing trigger for sparked block '%s'\n", block->sid);
			continue;
		}

		add_spark(state, block, trigger);
	}

	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		char *trigger = cfg_get_str(&state->sparks[i].other->cfg, BLOCK_OPT_TRIGGER);
		state->sparks[i].child = make_child(state, trigger, 0, 1, 0);
	}

	return state->num_sparks;
}

/*
 * Run a command in a 'fire and forget' manner. Does not invoke a shell,
 * hence no fancy stuff like pipes can be used with this.
 */
int run_cmd(const char *cmd)
{
	kita_child_s *child = kita_child_new(cmd, 0, 0, 0);
	kita_child_open(child);  // runs the child via fork/execvp
	kita_child_close(child); // does not stop the child, just closes com channels
	return 1; // TODO
}

/*
 * Takes a string that might represent an action that was registered with one 
 * of the blocks and tries to find the associated block. If found, the command
 * associated with the action will be executed.
 * Returns 0 on success, -1 if the string was not a recognized action command
 * or the block that the action belongs to could not be found.
 */
static int process_action(const state_s *state, const char *action)
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
	snprintf(type, 5, "%s", action + len - 4);

	// Extract everything _before_ the suffix (this is the block name)
	char block[len-3];
	snprintf(block, len - 3, "%s", action); 

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
	thing_s *source = get_block(state, block);
	if (source == NULL)
	{
		return -1;
	}

	// Now to fire the right command for the action type
	switch (b) {
		case 0:
			run_cmd(cfg_get_str(&source->cfg, BLOCK_OPT_CMD_LMB));
			return 0;
		case 1:
			run_cmd(cfg_get_str(&source->cfg, BLOCK_OPT_CMD_MMB));
			return 0;
		case 2:
			run_cmd(cfg_get_str(&source->cfg, BLOCK_OPT_CMD_RMB));
			return 0;
		case 3:
			run_cmd(cfg_get_str(&source->cfg, BLOCK_OPT_CMD_SUP));
			return 0;
		case 4:
			run_cmd(cfg_get_str(&source->cfg, BLOCK_OPT_CMD_SDN));
			return 0;
		default:
			// Should never happen...
			return -1;
	}
}

static void feed_lemon(state_s *state)
{
	if (state->due == 0)
	{
		return;
	}

	char *input = barstr(state);
	kita_child_feed(state->lemon.child, input);
	free(input);
	state->due = 0;
}

/*
 * This callback is supposed to be called for every block name that is being 
 * extracted from the config file's 'format' option for the bar itself, which 
 * lists the blocks to be displayed on the bar. `name` should contain the name 
 * of the block as read from the format string, `align` should be -1, 0 or 1, 
 * meaning left, center or right, accordingly (indicating where the block is 
 * supposed to be displayed on the bar).
 */
static void on_block_found(const char *name, int align, void *data)
{
	// 'Unpack' the data
	state_s *state = (state_s*) data;
	
	// Find or add the block with the given name
	thing_s *block = add_block(state, name);
	
	if (block == NULL)
	{
		fprintf(stderr, "on_block_found(): add_block() failed!\n");
		return;
	}
	// Set the block's align to the given one
	cfg_set_int(&block->cfg, BLOCK_OPT_ALIGN, align);
}

/*
 * Handles SIGINT (CTRL+C) and similar signals by setting the static variable
 * `running` to 0, effectively ending the main loop, so that clean-up happens.
 */
void on_signal(int sig)
{
	running = 0;
	handled = sig;
}

static thing_s *thing_by_child(state_s *state, kita_child_s *child)
{
	// lemon
	if (child == state->lemon.child)
	{
		return &state->lemon;
	}

	// blocks
	for (size_t i = 0; i < state->num_blocks; ++i)
	{
		if (child == state->blocks[i].child)
		{
			return &state->blocks[i];
		}
	}
	
	// sparks
	for (size_t i = 0; i < state->num_sparks; ++i)
	{
		if (child == state->sparks[i].child)
		{
			return &state->sparks[i];
		}
	}


	// not found
	return NULL;
}

void on_child_error(kita_state_s *ks, kita_event_s *ke)
{
	//fprintf(stderr, "on_child_error(): %s\n", ke->child->cmd);
	// TODO possibly log this to a file or something
}

void on_child_readok(kita_state_s *ks, kita_event_s *ke)
{
	//fprintf(stderr, "on_child_readok(): %s (%d bytes)\n", ke->child->cmd, ke->size);

	if (ke->size == 0)
	{
		return;
	}

	state_s *state = (state_s*) kita_child_get_context(ke->child);
	thing_s *thing = thing_by_child(state, ke->child);

	if (thing == NULL)
	{
		return;
	}

	if (thing->t_type == THING_LEMON)
	{
		if (ke->ios == KITA_IOS_OUT)
		{
			char *output = kita_child_read(ke->child, ke->ios);
			process_action(state, output);
			free(output);
		}
		else
		{
			// TODO user should be able to choose whether this ...
			//      - ... will be ignored
			//      - ... will be printed to stderr
			//      - ... will be logged to a file
			fprintf(stderr, "%s\n", kita_child_read(ke->child, ke->ios));
		}
		return;
	}

	if (thing->t_type == THING_BLOCK)
	{
		if (ke->ios == KITA_IOS_OUT)
		{
			// schedule an update if the block's output was
			// different from its previous output
			if (read_block(thing))
			{
				state->due = 1;
			}
		}
		else
		{
			// TODO implement block mode switch logic

		}
		return;
	}

	if (thing->t_type == THING_SPARK)
	{
		if (ke->ios == KITA_IOS_OUT)
		{
			read_spark(thing);
		}
		return;
	}
}

void on_child_closed(kita_state_s *ks, kita_event_s *ke)
{
	//fprintf(stderr, "on_child_closed(): %s\n", ke->child->cmd);
}

void on_child_exited(kita_state_s *ks, kita_event_s *ke)
{
	//fprintf(stderr, "on_child_exited(): %s\n", ke->child->cmd);
	
	state_s *state = (state_s*) kita_child_get_context(ke->child);
	thing_s *thing = thing_by_child(state, ke->child);

	if (thing == NULL)
	{
		return;
	}
	
	if (thing->t_type == THING_LEMON)
	{
		running = 0;
		return;
	}

	if (thing->t_type == THING_BLOCK)
	{
		thing->alive = 0;
		return;
	}
	
	if (thing->t_type == THING_SPARK)
	{
		thing->alive = 0;
		return;
	}
}

void on_child_reaped(kita_state_s *ks, kita_event_s *ke)
{
	//fprintf(stderr, "on_child_reaped(): %s\n", ke->child->cmd);
	on_child_exited(ks, ke);
}

static void cleanup(state_s *state)
{
	// free sparks
	free_sparks(state);
	free(state->sparks);
	state->sparks = NULL;
	state->num_sparks = 0;

	// free blocks
	free_blocks(state);
	free(state->blocks);
	state->blocks = NULL;
	state->num_blocks = 0;

	// free bar
	free_thing(&state->lemon);

	// free kita
	kita_free(&state->kita);
	state->kita = NULL;

	// misc
	state->due = 0;
}

// http://courses.cms.caltech.edu/cs11/material/general/usage.html
static void help(const char *invocation, FILE *where)
{
	fprintf(where, "USAGE\n");
	fprintf(where, "\t%s [OPTIONS...]\n", invocation);
	fprintf(where, "\n");
	fprintf(where, "OPTIONS\n");
	fprintf(where, "\t-c\tconfig file to use\n");
	fprintf(where, "\t-e\trun bar even if it is empty (no blocks)\n");
	fprintf(where, "\t-h\tprint this help text and exit\n");
	fprintf(where, "\t-s\tINI section name for the bar\n");
	fprintf(where, "\t-V\tprint version information and exit\n");
}

static void version()
{
	fprintf(stdout, "%s %d.%d.%d\n%s\n", SUCCADE_NAME,
			SUCCADE_VER_MAJOR, SUCCADE_VER_MINOR, SUCCADE_VER_PATCH,
			SUCCADE_URL);
}

int main(int argc, char **argv)
{
	//
	// SIGNAL HANDLING
	//

	struct sigaction sa_int = { .sa_handler = &on_signal };

	sigaction(SIGINT,  &sa_int, NULL);
	sigaction(SIGQUIT, &sa_int, NULL);
	sigaction(SIGTERM, &sa_int, NULL);
	sigaction(SIGPIPE, &sa_int, NULL);
	
	//
	// CHECK FOR X 
	//

	if (!x_is_running())
	{
		fprintf(stderr, "Failed to detect X\n");
		return EXIT_FAILURE;
	}

	//
	// SUCCADE STATE
	//

	state_s  state = { 0 };
	prefs_s *prefs = &(state.prefs); // For convenience
	thing_s *lemon = &(state.lemon); // For convenience

	//
	// KITA STATE
	//

	state.kita = kita_init();
	if (state.kita == NULL)
	{
		fprintf(stderr, "Failed to initialize kita state\n");
		return EXIT_FAILURE;
	}

	kita_state_s *kita = state.kita; // For convenience
	kita_set_option(kita, KITA_OPT_NO_NEWLINE, 1);

	// 
	// KITA CALLBACKS 
	//

	kita_set_callback(kita, KITA_EVT_CHILD_CLOSED, on_child_closed);
	kita_set_callback(kita, KITA_EVT_CHILD_REAPED, on_child_reaped);
	kita_set_callback(kita, KITA_EVT_CHILD_HANGUP, on_child_exited);
	kita_set_callback(kita, KITA_EVT_CHILD_EXITED, on_child_exited);
	kita_set_callback(kita, KITA_EVT_CHILD_READOK, on_child_readok);
	kita_set_callback(kita, KITA_EVT_CHILD_ERROR,  on_child_error);

	//
	// COMMAND LINE ARGUMENTS
	//

	parse_args(argc, argv, prefs);
	char *default_cfg_path = NULL;

	//
	// PRINT HELP AND EXIT, MAYBE
	//

	if (prefs->help)
	{
		help(argv[0], stdout);
		return EXIT_SUCCESS;
	}

	// PRINT VERSION AND EXIT, MAYBE
	if (prefs->version)
	{
		version();
		return EXIT_SUCCESS;
	}

	//
	// PREFERENCES / DEFAULTS
	//

	// if no custom config file given, set it to the default
	if (prefs->config == NULL)
	{
		// we use an additional variable for consistency with free()
		default_cfg_path = config_path(DEFAULT_CFG_FILE, SUCCADE_NAME);
		prefs->config = default_cfg_path; 
	}

	// if no custom INI section for bar given, set it to default
	if (prefs->section == NULL)
	{
		prefs->section = DEFAULT_LEMON_SECTION;
	}

	//
	// BAR
	//

	// copy the section ID from the config for convenience and consistency
	lemon->sid = strdup(prefs->section);
	lemon->t_type = THING_LEMON;
	cfg_init(&lemon->cfg, "lemon", LEMON_OPT_COUNT);

	// read the config file and parse bar's section
	if (load_lemon_cfg(&state) < 0)
	{
		fprintf(stderr, "Failed to load config file: %s\n", prefs->config);
		return EXIT_FAILURE;
	}
	
	// if no `bin` option was present in the config, set it to the default
	if (!cfg_has(&lemon->cfg, LEMON_OPT_BIN))
	{
		// We use strdup() for consistency with free() later on
		cfg_set_str(&lemon->cfg, LEMON_OPT_BIN, strdup(DEFAULT_LEMON_BIN));
	}

	// if no `name` option was present in the config, set it to the default
	if (!cfg_has(&lemon->cfg, LEMON_OPT_NAME))
	{
		// We use strdup() for consistency with free() later on
		cfg_set_str(&lemon->cfg, LEMON_OPT_NAME, strdup(DEFAULT_LEMON_NAME));
	}

	// if no 'areas' option was present in the config, set it to the default
	if (!cfg_has(&lemon->cfg, LEMON_OPT_AREAS))
	{
		cfg_set_int(&lemon->cfg, LEMON_OPT_AREAS, DEFAULT_LEMON_AREAS);
	}

	// create the child process and add it to the kita state
	char *lemon_bin = cfg_get_str(&lemon->cfg, LEMON_OPT_BIN);
	lemon->child = make_child(&state, lemon_bin, 1, 1, 1);
	if (lemon->child == NULL)
	{
		fprintf(stderr, "Failed to create bar process: %s\n", lemon_bin);
		return EXIT_FAILURE;
	}

	// open (run) the lemon
	if (open_lemon(lemon) == -1)
	{
		fprintf(stderr, "Failed to open bar: %s\n", lemon->sid);
		return EXIT_FAILURE;
	}

	//
	// BLOCKS
	//

	// create blocks by parsing the format string
	// TODO I'd like to make this into a two-step thing:
	//      1. parse the format string, creating an array of requested block names
	//      2. iterate through the requested block names, creating blocks as we go
	char *lemon_format = cfg_get_str(&lemon->cfg, LEMON_OPT_FORMAT);
	parse_format(lemon_format, on_block_found, &state);
	
	// exit if no blocks could be loaded and 'empty' option isn't present
	if (state.num_blocks == 0 && prefs->empty == 0)
	{
		fprintf(stderr, "Failed to load any blocks\n");
		return EXIT_FAILURE;
	}

	// parse the config again, this time processing block sections
	if (load_block_cfg(&state) < 0)
	{
		fprintf(stderr, "Failed to load config file: %s\n", prefs->config);
		return EXIT_FAILURE;
	}

	// create child processes and add them to the kita state
	thing_s *block = NULL;
	for (size_t i = 0; i < state.num_blocks; ++i)
	{
		block = &state.blocks[i];
		char *block_bin = cfg_get_str(&block->cfg, BLOCK_OPT_BIN);
		char *block_cmd = block_bin ? block_bin : block->sid;
		block->child = make_child(&state, block_cmd, 0, 1, 1);
	}

	//
	// SPARKS
	//

	create_sparks(&state);
	open_sparks(&state);
	
	//
	// MAIN LOOP
	//

	double now;
	//double before = get_time();
	//double delta;
	double wait = 0.0; 

	running = 1;
	
	while (running)
	{
		// update time (passed)
		now    = get_time();
		//delta  = now - before;
		//before = now;

		//fprintf(stderr, "> now = %f, wait = %f, delta = %f\n", now, wait, delta);

		// open all blocks that are due for (another) invocation
		open_due_blocks(&state, now);

		// feed lemon (if the state's 'due' field is set)
		feed_lemon(&state);

		// let kita check for child events (for up to `wait` seconds)
		kita_tick(kita, (wait == -1 ? wait : wait * MILLISEC_PER_SEC));

		// figure out how long we can idle, based on timed blocks
		wait = time_to_wait(&state, now);
	}

	//
	// CLEAN UP
	//

	cleanup(&state);
	free(default_cfg_path);

	return EXIT_SUCCESS;
}

