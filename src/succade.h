#ifndef SUCCADE_H
#define SUCCADE_H

#include "libkita.h"
#include <unistd.h> // STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO

#define DEBUG 0

#define SUCCADE_NAME "succade"
#define SUCCADE_URL  "https://github.com/domsson/succade"
#define SUCCADE_VER_MAJOR 1
#define SUCCADE_VER_MINOR 2
#define SUCCADE_VER_PATCH 1

#define BUFFER_NUMERIC          8
#define BUFFER_LEMON_ARG     1024

#define BUFFER_BLOCK_NAME      64
#define BUFFER_BLOCK_RESULT   256
#define BUFFER_BLOCK_STR     2048

#define BLOCK_WAIT_TOLERANCE 0.1
#define MILLISEC_PER_SEC     1000

#define DEFAULT_CFG_FILE "succaderc"

#define DEFAULT_LEMON_BIN     "lemonbar"
#define DEFAULT_LEMON_NAME    "succade_lemonbar"
#define DEFAULT_LEMON_SECTION "bar"
#define DEFAULT_LEMON_AREAS   10

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
	LEMON_OPT_NAME,
	LEMON_OPT_BIN,
	LEMON_OPT_WIDTH,
	LEMON_OPT_HEIGHT,
	LEMON_OPT_X,
	LEMON_OPT_Y,
	LEMON_OPT_BOTTOM,
	LEMON_OPT_FORCE,
	LEMON_OPT_AREAS,
	LEMON_OPT_FORMAT,
	LEMON_OPT_FG, // block
	LEMON_OPT_BG,
	LEMON_OPT_LW,
	LEMON_OPT_LC, // block
	LEMON_OPT_OL, // block
	LEMON_OPT_UL, // block
	LEMON_OPT_BLOCK_BG, // block
	LEMON_OPT_LABEL_FG, // block
	LEMON_OPT_LABEL_BG, // block
	LEMON_OPT_AFFIX_FG, // block
	LEMON_OPT_AFFIX_BG, // block
	LEMON_OPT_BLOCK_MARGIN, // block
	LEMON_OPT_BLOCK_PADDING, // block
	LEMON_OPT_BLOCK_PREFIX, // block
	LEMON_OPT_BLOCK_SUFFIX, // block
	LEMON_OPT_BLOCK_FONT,
	LEMON_OPT_LABEL_FONT,
	LEMON_OPT_AFFIX_FONT,
	LEMON_OPT_COUNT
};

enum succade_block_opt
{
	BLOCK_OPT_BIN,
	BLOCK_OPT_FG,
	BLOCK_OPT_BG,
	BLOCK_OPT_LABEL_FG,
	BLOCK_OPT_LABEL_BG,
	BLOCK_OPT_AFFIX_FG,
	BLOCK_OPT_AFFIX_BG,
	BLOCK_OPT_LC,
	BLOCK_OPT_OL,
	BLOCK_OPT_UL,
	BLOCK_OPT_WIDTH,
	BLOCK_OPT_MARGIN_LEFT,
	BLOCK_OPT_MARGIN_RIGHT,
	BLOCK_OPT_ALIGN,
	BLOCK_OPT_PREFIX,
	BLOCK_OPT_SUFFIX,
	BLOCK_OPT_LABEL,
	BLOCK_OPT_UNIT,
	BLOCK_OPT_TRIGGER,
	BLOCK_OPT_CONSUME,
	BLOCK_OPT_RELOAD,
	BLOCK_OPT_LIVE,
	BLOCK_OPT_RAW,
	BLOCK_OPT_CMD_LMB,
	BLOCK_OPT_CMD_MMB,
	BLOCK_OPT_CMD_RMB,
	BLOCK_OPT_CMD_SUP,
	BLOCK_OPT_CMD_SDN,
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
	thing_s *blocks;         // Reference to block array
	thing_s *sparks;         // Reference to spark array (prev. 'trigger')
	size_t   num_blocks;     // Number of blocks in blocks array
	size_t   num_sparks;     // Number of sparks in sparks array
	kita_state_s *kita;
	unsigned char due : 1;
};

typedef void (*create_block_callback)(const char *name, int align, void *data);

#endif
