#ifndef SUCCADE_H
#define SUCCADE_H

#include "libkita.h"
#include <unistd.h> // STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO

#define DEBUG 1

#define SUCCADE_NAME "succade"
#define SUCCADE_URL  "https://github.com/domsson/succade"
#define SUCCADE_VER_MAJOR 0
#define SUCCADE_VER_MINOR 3
#define SUCCADE_VER_PATCH 0

#define BUFFER_LEMON_ARG   1024
#define BUFFER_LEMON_INPUT 2048
#define BUFFER_BLOCK_NAME    64

#define BLOCK_WAIT_TOLERANCE 0.1
#define MILLISEC_PER_SEC     1000

#define DEFAULT_CFG_FILE "succaderc"

#define DEFAULT_LEMON_BIN     "lemonbar"
#define DEFAULT_LEMON_NAME    "succade_lemonbar"
#define DEFAULT_LEMON_SECTION "bar"

//
// ENUMS
//

enum succade_child_type
{
	CHILD_LEMON,
	CHILD_BLOCK,
	CHILD_SPARK
};

enum succade_block_type
{
	BLOCK_NONE = -1,
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

typedef enum succade_child_type child_type_e;
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
	LEMON_OPT_BLOCK_OFFSET, // block
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
	BLOCK_OPT_BLOCK_FG,
	BLOCK_OPT_BLOCK_BG,
	BLOCK_OPT_LABEL_FG,
	BLOCK_OPT_LABEL_BG,
	BLOCK_OPT_AFFIX_FG,
	BLOCK_OPT_AFFIX_BG,
	BLOCK_OPT_LC,
	BLOCK_OPT_OL,
	BLOCK_OPT_UL,
	BLOCK_OPT_WIDTH,
	BLOCK_OPT_OFFSET,
	BLOCK_OPT_ALIGN,
	BLOCK_OPT_PREFIX,
	BLOCK_OPT_SUFFIX,
	BLOCK_OPT_LABEL,
	BLOCK_OPT_UNIT,
	BLOCK_OPT_TRIGGER,
	BLOCK_OPT_CONSUME,
	BLOCK_OPT_RELOAD,
	BLOCK_OPT_LIVE,
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
struct succade_lemon;
struct succade_block;
struct succade_spark;
struct succade_prefs;
struct succade_state;

typedef struct succade_thing thing_s;
typedef struct succade_lemon lemon_s;
typedef struct succade_block block_s;
typedef struct succade_spark spark_s;
typedef struct succade_prefs prefs_s;
typedef struct succade_state state_s;

struct succade_lemon
{
	char         *sid;       // section ID (config section name)
	kita_child_s *child;     // associated child process
	cfg_s         cfg;
};

struct succade_block
{
	char         *sid;       // section ID (config section name)
	kita_child_s *child;     // associated child process
	block_type_e  type;      // type of block (one-shot, reload, sparked, live)
	cfg_s         cfg;
	spark_s      *spark;     // asosciated spark, if any

	char         *output;
	double        last_open; // time of last invocation (0.0 for never)
	double        last_read; // time of last read from stdout (TODO what about stderr)
	unsigned char alive : 1;
};

struct succade_spark
{
	kita_child_s *child;     // associated child process
	block_s      *block;     // associated lemon or block struct
	char         *output;

	double last_open;      // time of last invocation (0.0 for never)
	double last_read;      // time of last read from stdout (TODO what about stderr)
	unsigned char alive : 1;
};

struct succade_thing
{
	char         *sid;
	cfg_s         cfg;

	kita_child_s *child;

	child_type_e  t_type;  // LEMON, BLOCK, SPARK
	block_type_e  b_type;  // BLOCK_ONCE, BLOCK_TIMED, BLOCK_SPARKED, BLOCK_LIVE
	thing_s      *other;   // associated block (for sparks) or spark (for blocks) 

	char         *output;
	unsigned char alive : 1;
	double        last_open;
	double        last_read;
};

struct succade_prefs
{
	char     *config;      // Full path to config file
	char     *section;     // INI section name for the bar
	unsigned  empty : 1;   // Run bar even if no blocks present?
	unsigned  help  : 1;   // Show help text and exit?
};

struct succade_state
{
        prefs_s  prefs;        // Preferences (options/config)
	//lemon_s  lemon;        // Lemon (prev. 'bar')
	thing_s  lemon;
	block_s *blocks;       // Reference to block array
	spark_s *sparks;       // Reference to spark array (prev. 'trigger')
	size_t   num_blocks;   // Number of blocks in blocks array
	size_t   num_sparks;   // Number of sparks in sparks array
	kita_state_s *kita;
	unsigned char due : 1;
};

typedef void (*create_block_callback)(const char *name, int align, int n, void *data);

#endif
