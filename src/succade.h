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
	LEMON_OPT_BG,
	LEMON_OPT_LW,
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
	BLOCK_OPT_COUNT
};

enum succade_click_opt
{
	CLICK_OPT_LMB,
	CLICK_OPT_MMB,
	CLICK_OPT_RMB,
	CLICK_OPT_SUP,
	CLICK_OPT_SDN,
	CLICK_OPT_COUNT
};

typedef enum succade_lemon_opt lemon_opt_e;
typedef enum succade_block_opt block_opt_e;
typedef enum succade_click_opt click_opt_e;

//
// UNIONS
// 

union succade_cfg_value
{
	char *s;
	int   i;
	float f;
};

typedef union succade_cfg_value cfg_value_u;

//
// STRUCTS
//

struct succade_lemon;
struct succade_block;
struct succade_spark;
struct succade_prefs;
struct succade_state;

struct succade_lemon_cfg;
struct succade_block_cfg;
struct succade_click_cfg;

typedef struct succade_lemon lemon_s;
typedef struct succade_block block_s;
typedef struct succade_spark spark_s;
typedef struct succade_prefs prefs_s;
typedef struct succade_state state_s;

typedef struct succade_lemon_cfg lemon_cfg_s;
typedef struct succade_block_cfg block_cfg_s;
typedef struct succade_click_cfg click_cfg_s;

struct succade_lemon_cfg
{
	char *name;            // Window name (WM_NAME)
	char *bin;             // Command to run

	size_t w : 16;         // Width of the bar
	size_t h : 16;         // Height of the bar
	size_t x : 16;         // x-position of the bar
	size_t y : 16;         // y-position of the bar

	unsigned bottom : 1;   // Position bar at bottom of screen?
	unsigned force : 1;    // Force docking?
	char *format;          // List and position of blocks

	char *bg;              // Background color for the entire bar
	size_t lw : 8;         // Overline/underline width in px

	char *block_font;      // The default font to use (slot 1)
	char *label_font;      // Font used for the label (slot 2)
	char *affix_font;      // Font used for prefix/suffix (slot 3)
};

struct succade_block_cfg
{
	char *bin;             // Command to run

	char *fg;              // Foreground color
	char *bg;              // Background color
	char *label_fg;        // Foreground color for the label
	char *label_bg;        // Background color for the label
	char *affix_fg;        // Foreground color for the affixes
	char *affix_bg;        // Background color for the affixes

	char *lc;              // Overline/underline color
	unsigned ol : 1;       // Draw overline?
	unsigned ul : 1;       // Draw underline?

	size_t   width  :  8;  // Minimum width of result in chars (previously 'padding')
	int      offset : 16;  // Offset to next block in px
	int      align;        // -1, 0, 1 (left, center, right)
	
	char *prefix;          // Prepend this to the block's result [TODO] this was previously on bar-level only, implement
	char *suffix;          // Append this to the block's result  [TODO] this was previously on bar-level only, implement
	char *label;           // Prefixes the result string
	char *unit;            // Will be appended to the result string [TODO] implement

	char     *trigger;     // Run block based on this cmd's output
	unsigned  consume : 1; // Consume the trigger's output, if any
	double    reload;      // Interval between runs, in seconds 
	unsigned  live    : 1; // This block is its own trigger
};

struct succade_click_cfg
{
	char *lmb;             // Command to run on left mouse click
	char *mmb;             // Command to run on middle mouse click
	char *rmb;             // Command to run on right mouse click
	char *sup;             // Command to run on scroll up
	char *sdn;             // Command to run on scroll down
};

struct succade_lemon
{
	char         *sid;       // section ID (config section name)
	kita_child_s *child;     // associated child process
	lemon_cfg_s   lemon_cfg; // associated lemon config
	block_cfg_s   block_cfg; // associated common block config
	cfg_value_u  *cfg;
};

struct succade_block
{
	char         *sid;       // section ID (config section name)
	kita_child_s *child;     // associated child process
	block_type_e  type;      // type of block (one-shot, reload, sparked, live)
	block_cfg_s   block_cfg; // associated block config
	click_cfg_s   click_cfg; // associated action commands
	spark_s      *spark;     // asosciated spark, if any

	char         *output;
	double last_open;      // time of last invocation (0.0 for never)
	double last_read;      // time of last read from stdout (TODO what about stderr)
	unsigned char alive : 1;
};

struct succade_spark
{
	kita_child_s *child;     // associated child process
	block_s      *block;     // associated lemon or block struct
	char         *output;

	double last_open;      // time of last invocation (0.0 for never)
	double last_read;      // time of last read from stdout (TODO what about stderr)
	unsigned char alive: 1;
};

/*
struct succade_thing
{
	char         *sid;
	cfg_value_u  *cfg;

	kita_child_s *child;

	thing_type_e  type; // LEMON, BLOCK_ONCE, BLOCK_TIMED, BLOCK_SPARKED, BLOCK_LIVE, SPARK
	thing_s      *other;    // associated block or spark, depending on what this is

	char         *output;
	unsigned char alive : 1;
	double        last_open;
	double        last_read;
};
*/

struct succade_prefs
{
	char     *config;      // Full path to config file
	char     *section;     // INI section name for the bar
	unsigned  empty : 1;   // Run bar even if no blocks present?
	unsigned  help  : 1;   // Show help text and exit?
};

struct succade_state
{
        prefs_s  prefs;    // Preferences (options/config)
	lemon_s  lemon;    // Lemon (prev. 'bar')
	block_s *blocks;   // Reference to block array
	spark_s *sparks;   // Reference to spark array (prev. 'trigger')
	size_t num_blocks; // Number of blocks in blocks array
	size_t num_sparks; // Number of sparks in sparks array
	kita_state_s *kita;
	unsigned char due : 1;
};

typedef void (*create_block_callback)(const char *name, int align, int n, void *data);

#endif
