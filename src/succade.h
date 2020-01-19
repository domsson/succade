#ifndef SUCCADE_H
#define SUCCADE_H

#define DEBUG 1 
#define NAME "succade"

#define BUFFER_SIZE 2048
#define BLOCK_NAME_MAX 64

#define BLOCK_WAIT_TOLERANCE 0.1
#define MILLISEC_PER_SEC     1000

#define DEFAULT_LEMON_BIN     "lemonbar"
#define DEFAULT_LEMON_NAME    "succade_lemonbar"
#define DEFAULT_LEMON_SECTION "bar"

enum succade_event_type
{
	EV_LEMON,
	EV_BLOCK,
	EV_SPARK
};

/*
   // TODO just use these instead?
	STDIN_FILENO is a macro defined in <unistd.h>. At least for a POSIX 
	compliant system, it's required to be defined as 0. Similarly, 
	STDOUT_FILENO is 1 and STDERR_FILENO is 2.
*/

enum succade_fd_type
{
	FD_IN,
	FD_OUT,
	FD_ERR
};

typedef enum succade_event_type scd_ev_type_e;
typedef enum succade_fd_type    scd_fd_type_e;

struct succade_lemon;
struct succade_block;
struct succade_spark;
struct succade_prefs;
struct succade_state;
struct succade_event;

typedef struct succade_lemon scd_lemon_s;
typedef struct succade_block scd_block_s;
typedef struct succade_spark scd_spark_s;
typedef struct succade_prefs scd_prefs_s;
typedef struct succade_state scd_state_s;
typedef struct succade_event scd_event_s;

struct succade_lemon
{
	char *name;            // Name of the bar (will be used as window title)
	char *bin;             // Binary for launching bar (default: `lemonbar`)
	pid_t pid;             // Process ID of the bar 
	FILE *fp[3];           // File pointers (stdin, stdout, stderr)
	char *fg;              // Foreground color
	char *bg;              // Background color
	char *lc;              // Overline/underline color
	char *prefix;          // Prepend this to every block's result
	char *suffix;          // Append this to every block's result
	unsigned ol : 1;       // Draw overline for all blocks?
	unsigned ul : 1;       // Draw underline for all blocks?
	size_t lw : 8;         // Overline/underline width in px
	size_t w : 16;         // Width of the bar
	size_t h : 16;         // Height of the bar
	size_t x : 16;         // x-position of the bar
	size_t y : 16;         // y-position of the bar
	unsigned bottom : 1;   // Position bar at bottom of screen?
	unsigned force : 1;    // Force docking?
	int offset : 16;       // Offset between any two blocks 
	char *format;          // List and position of blocks
	char *block_font;      // The default font to use (slot 1)
	char *label_font;      // Font used for the label (slot 2)
	char *affix_font;      // Font used for prefix/suffix (slot 3)
	char *block_bg;        // Background color for all blocks
	char *label_fg;        // Foreground color for all labels
	char *label_bg;        // Background color for all labels
	char *affix_fg;        // Foreground color for all affixes
	char *affix_bg;        // Background color for all affixes
};

struct succade_block
{
	char *name;            // Name of the block 
	char *bin;             // Command/binary/script to run 
	pid_t pid;             // Process ID of this block's process
	FILE *fp[3];           // File pointers for stdin, stdout, stderr
	char *fg;              // Foreground color
	char *bg;              // Background color
	char *label_fg;        // Foreground color for the label
	char *label_bg;        // Background color for the label
	char *affix_fg;        // Foreground color for the affixes
	char *affix_bg;        // Background color for the affixes
	char *lc;              // Overline/underline color
	unsigned ol : 1;       // Draw overline?
	unsigned ul : 1;       // Draw underline?
	size_t padding : 8;    // Minimum width of result in chars
	int offset : 16;       // Offset to next block in px
	unsigned align;        // -1, 0, 1 (left, center, right)
	char *label;           // Prefixes the result string
	char *trigger;         // Run block based on this cmd's output
	char *cmd_lmb;         // Command to run on left mouse click
	char *cmd_mmb;         // Command to run on middle mouse click
	char *cmd_rmb;         // Command to run on right mouse click
	char *cmd_sup;         // Command to run on scroll up
	char *cmd_sdn;         // Command to run on scroll down
	unsigned live : 1;     // This block is its own trigger
	unsigned used : 1;     // Has this block been run at least once?
	double reload;         // Interval between runs 
	double waited;         // Time the block hasn't been run
	char *input;           // Recent output of the associated trigger
	char *result;          // Output of the most recent block run
};

struct succade_spark
{
	char *cmd;             // Command to run
	pid_t pid;             // Process ID of trigger command
	FILE *fp[3];           // File pointers for stdin, stdout, stderr
	scd_block_s *block;    // Associated block
	scd_lemon_s *lemon;    // Associated bar (special use case...)
	unsigned ready : 1;    // fd has new data available for reading
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
        scd_prefs_s *prefs;    // Reference to preferences (options/config)
	scd_lemon_s *lemon;    // Reference to lemon (prev. 'bar')
	scd_block_s *blocks;   // Reference to block array
	scd_spark_s *sparks;   // Reference to spark array (prev. 'trigger')
	scd_event_s *events;   // Reference to events array
	size_t num_blocks;     // Number of blocks in blocks array
	size_t num_sparks;     // Number of sparks in sparks array
	size_t num_events;     // Number of events in events array
	int epfd;              // epoll file descriptor
};

struct succade_event
{
	scd_fd_type_e fd_type;   // stdin, stdout, stderr?
	scd_ev_type_e ev_type;   // Type of data
	void *data;              // Ptr to lemon, a block or a spark
	int fd;                  // File descriptor of data->fp[0], [1] or [2]
	unsigned registered : 1; // Registered with epoll?
};

typedef void (*create_block_callback)(const char *name, int align, int n, void *data);

#endif
