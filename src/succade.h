#ifndef SUCCADE_H
#define SUCCADE_H

#include <unistd.h> // STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO

#define DEBUG 1

#define SUCCADE_NAME "succade"
#define SUCCADE_URL  "https://github.com/domsson/succade"
#define SUCCADE_VER_MAJOR 0
#define SUCCADE_VER_MINOR 3
#define SUCCADE_VER_PATCH 0

#define BUFFER_SIZE 2048
#define BLOCK_NAME_MAX 64

#define BLOCK_WAIT_TOLERANCE 0.1
#define MILLISEC_PER_SEC     1000

#define DEFAULT_CFG_FILE "succaderc"

#define DEFAULT_LEMON_BIN     "lemonbar"
#define DEFAULT_LEMON_NAME    "succade_lemonbar"
#define DEFAULT_LEMON_SECTION "bar"

/*
 * ENUMS
 */

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

/*
 * STRUCTS
 */

struct succade_child;
struct succade_lemon;
struct succade_block;
struct succade_spark;
struct succade_prefs;
struct succade_state;
struct succade_event;

struct succade_lemon_cfg;
struct succade_block_cfg;
struct succade_click_cfg;

typedef struct succade_child child_s;
typedef struct succade_lemon lemon_s;
typedef struct succade_block block_s;
typedef struct succade_spark spark_s;
typedef struct succade_prefs prefs_s;
typedef struct succade_state state_s;
typedef struct succade_event event_s;

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
	unsigned align;        // -1, 0, 1 (left, center, right)
	
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

struct succade_child
{
	char *cmd;             // command/binary to run (could have arguments)
	char *arg;             // additional argument string (optional)
	pid_t pid;             // process ID
	FILE *fp[3];           // stdin/stdout/stderr file pointers

	char *output;          // output of the last invocation

	double last_run;       // time of last invocation (0.0 for never)
	double last_read;      // time of last read from stdout (TODO what about stderr)
	unsigned ready : 1;    // fd has new data available for reading TODO maybe make it int and save the fp index that is ready?

	child_type_e  type;    // type of data: lemon, block or spark
	void         *thing;   // associated lemon, block or spark struct 
};

struct succade_lemon
{
	char         *sid;       // section ID (config section name)
	child_s       child;     // associated child process
	lemon_cfg_s   lemon_cfg; // associated lemon config
	block_cfg_s   block_cfg; // associated common block config
};

struct succade_block
{
	char         *sid;       // section ID (config section name)
	child_s       child;     // associated child process
	block_type_e  type;      // type of block (one-shot, reload, sparked, live)
	block_cfg_s   block_cfg; // associated block config
	click_cfg_s   click_cfg; // associated action commands
	spark_s      *spark;     // asosciated spark, if any
};

struct succade_spark
{
	child_s       child;     // associated child process
	block_s      *block;     // associated lemon or block struct
};

struct succade_event
{
	fdesc_type_e fd_type;    // stdin, stdout, stderr?
	child_type_e ev_type;    // Type of data
	void *thing;             // Ptr to lemon, a block or a spark
	int fd;                  // File descriptor of data->fp[0], [1] or [2]
	unsigned registered : 1; // Registered with epoll?
	unsigned dirty : 1;      // Unhandled activity has occurred
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
        prefs_s  prefs;    // Preferences (options/config)
	lemon_s  lemon;    // Lemon (prev. 'bar')
	block_s *blocks;   // Reference to block array
	spark_s *sparks;   // Reference to spark array (prev. 'trigger')
	event_s *events;   // Reference to events array
	size_t num_blocks; // Number of blocks in blocks array
	size_t num_sparks; // Number of sparks in sparks array
	size_t num_events; // Number of events in events array
	int epfd;          // epoll file descriptor
};

typedef void (*create_block_callback)(const char *name, int align, int n, void *data);

#endif
