#ifndef SUCCADE_H
#define SUCCADE_H

#include "libkita.h"
#include <unistd.h> // STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO

#define DEBUG 0

#define SUCCADE_NAME "succade"
#define SUCCADE_URL  "https://github.com/domsson/succade"
#define SUCCADE_VER_MAJOR 2
#define SUCCADE_VER_MINOR 1
#define SUCCADE_VER_PATCH 3

#define BUFFER_NUMERIC          8
#define BUFFER_LEMON_ARG     1024

#define BUFFER_BLOCK_NAME      64
#define BUFFER_BLOCK_RESULT   256
#define BUFFER_BLOCK_STR     2048

#define BLOCK_WAIT_TOLERANCE 0.1
#define MILLISEC_PER_SEC     1000

#define DEFAULT_CFG_FILE "succaderc"

#define ALBEDO_SID "default"

#define DEFAULT_LEMON_BIN     "lemonbar"
#define DEFAULT_LEMON_NAME    "succade_lemonbar"
#define DEFAULT_LEMON_SECTION "bar"

//
// ENUMS
//

enum succade_thing_type
{
	THING_LEMON,
	THING_BLOCK,
	THING_SPARK
};

enum succade_block_type
{
	BLOCK_NONE,
	BLOCK_ONCE,
	BLOCK_TIMED,
	BLOCK_SPARKED,
	BLOCK_LIVE
};

enum succade_fdesc_type
{
	FD_IN  = STDIN_FILENO,
	FD_OUT = STDOUT_FILENO,
	FD_ERR = STDERR_FILENO
};

typedef enum succade_thing_type thing_type_e;
typedef enum succade_block_type block_type_e;
typedef enum succade_fdesc_type fdesc_type_e;

enum succade_lemon_opt
{
	LEMON_OPT_BIN,         // lemonbar binary
	LEMON_OPT_FORMAT,      // blocks to display
	LEMON_OPT_WIDTH,       // -g: lemonbar width
	LEMON_OPT_HEIGHT,      // -g: lemonbar height
	LEMON_OPT_X,           // -g: lemonbar x pos
	LEMON_OPT_Y,           // -g: lemonbar y pos
	LEMON_OPT_BOTTOM,      // -b: dock at bottom
	LEMON_OPT_FORCE,       // -d: force docking
	LEMON_OPT_BLOCK_FONT,  // -f: font for blocks
	LEMON_OPT_LABEL_FONT,  // -f: font for blocks' labels
	LEMON_OPT_AFFIX_FONT,  // -a: font for blocks' affixes
	LEMON_OPT_AREAS,       // -a: number of clickable areas
	LEMON_OPT_NAME,        // -n: WM_NAME
	LEMON_OPT_LW,          // -u: underline width
	LEMON_OPT_BG,          // -B: default background color
	LEMON_OPT_FG,          // -F: default foreground color
	LEMON_OPT_LC,          // -U: underline color
	LEMON_OPT_SEPARATOR,   // string to separate blocks with
	LEMON_OPT_COUNT
};

enum succade_block_opt
{
	BLOCK_OPT_BIN,           // string: binary
	BLOCK_OPT_FG,            // color: foreground
	BLOCK_OPT_BG,            // color: background
	BLOCK_OPT_LABEL_FG,      // color: label foreground
	BLOCK_OPT_LABEL_BG,      // color: label backgorund
	BLOCK_OPT_AFFIX_FG,      // color: affix foreground
	BLOCK_OPT_AFFIX_BG,      // color: affix background
	BLOCK_OPT_LC,            // color: underline / overline
	BLOCK_OPT_OL,            // bool: draw overline
	BLOCK_OPT_UL,            // bool: draw underline
	BLOCK_OPT_MIN_WIDTH,     // int: minimum result width
	BLOCK_OPT_MARGIN_LEFT,   // int: margin left 
	BLOCK_OPT_MARGIN_RIGHT,  // int: margin right
	BLOCK_OPT_PADDING_LEFT,  // int: padding left
	BLOCK_OPT_PADDING_RIGHT, // int: padding right
	BLOCK_OPT_ALIGN,         // string: 'left', 'center' or 'right' 
	BLOCK_OPT_PREFIX,        // string: prefix
	BLOCK_OPT_SUFFIX,        // string: suffix
	BLOCK_OPT_LABEL,         // string: label
	BLOCK_OPT_UNIT,          // string: unit
	BLOCK_OPT_TRIGGER,       // string: trigger binary
	BLOCK_OPT_CONSUME,       // bool: consume trigger output
	BLOCK_OPT_RELOAD,        // bool: reload if dead
	BLOCK_OPT_LIVE,          // bool: live (keeps running)
	BLOCK_OPT_RAW,           // bool: don't escape '%'
	BLOCK_OPT_CMD_LMB,       // string: run on left click
	BLOCK_OPT_CMD_MMB,       // string: run on middle click
	BLOCK_OPT_CMD_RMB,       // string: run on right click
	BLOCK_OPT_CMD_SUP,       // string: run on scroll up
	BLOCK_OPT_CMD_SDN,       // string: run on scroll down
	BLOCK_OPT_COUNT
};

typedef enum succade_lemon_opt lemon_opt_e;
typedef enum succade_block_opt block_opt_e;

//
// STRUCTS
//

struct succade_thing;
struct succade_prefs;
struct succade_state;

typedef struct succade_thing thing_s;
typedef struct succade_prefs prefs_s;
typedef struct succade_state state_s;

struct succade_thing
{
	char         *sid;       // section ID (config section name)
	cfg_s         cfg;       // holds the config's options

	kita_child_s *child;     // kita child process struct

	thing_type_e  t_type;    // thing type (lemon, block, spark?) 
	block_type_e  b_type;    // block type (once, timed, sparked, live?)
	thing_s      *other;     // associated block (for sparks) or spark (for blocks) 

	char         *output;    // last output from stdout
	unsigned char alive : 1; // is up and running?
	double        last_open; // timestamp (in seconds) of last open operation
	double        last_read; // timestamp (in seconds) of last read operation
};

struct succade_prefs
{
	char     *config;        // Full path to config file
	char     *section;       // INI section name for the bar
	unsigned char empty : 1; // Run bar even if no blocks present?
	unsigned char help  : 1; // Show help text and exit?
	unsigned char version : 1; // Show version and exit?
};

struct succade_state
{
        prefs_s  prefs;          // Preferences (options/config)
	thing_s  lemon;
	thing_s  albedo;         // Dummy block for the default configuration
	thing_s *blocks;         // Reference to block array
	thing_s *sparks;         // Reference to spark array (prev. 'trigger')
	size_t   num_blocks;     // Number of blocks in blocks array
	size_t   num_sparks;     // Number of sparks in sparks array
	kita_state_s *kita;
	unsigned char due : 1;
};

typedef void (*create_block_callback)(const char *name, int align, void *data);

#endif
