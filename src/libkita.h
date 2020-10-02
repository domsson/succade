#ifndef LIBKITA_H
#define LIBKITA_H

#include <stdio.h>  // _IONBF, _IOLBF, _IOFBF
#include <unistd.h> // STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  API                                                                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Program information
#define KITA_NAME      "libkita"
#define KITA_URL       "https://github.com/domsson/libkita"
#define KITA_VER_MAJOR  0
#define KITA_VER_MINOR  2
#define KITA_VER_PATCH  0

// Buffers etc
#define KITA_BUFFER_SIZE 2048
#define KITA_MS_PER_S    1000

// Errors
#define KITA_ERR_NONE              0
#define KITA_ERR_OTHER            -1
#define KITA_ERR_EPOLL_CREATE     -8
#define KITA_ERR_EPOLL_CTL        -9
#define KITA_ERR_EPOLL_WAIT      -10 // epoll_pwait() error
#define KITA_ERR_EPOLL_SIG       -11 // epoll_pwait() caught a signal
#define KITA_ERR_WAIT            -20 // wait(), waitpid() or waidid() error
#define KITA_ERR_CHILD_UNKNOWN   -30
#define KITA_ERR_CHILD_TRACKED   -31
#define KITA_ERR_CHILD_UNTRACKED -32
#define KITA_ERR_CHILD_OPEN      -33
#define KITA_ERR_CHILD_CLOSED    -34
#define KITA_ERR_CHILD_ALIVE     -35
#define KITA_ERR_CHILD_DEAD      -36
#define KITA_ERR_READ            -40
#define KITA_ERR_READ_EOF        -41

//
// ENUMS 
//

enum kita_ios_type {
	KITA_IOS_NONE = -1,
	KITA_IOS_IN   = STDIN_FILENO,  // 0
	KITA_IOS_OUT  = STDOUT_FILENO, // 1
	KITA_IOS_ERR  = STDERR_FILENO, // 2
	KITA_IOS_ALL
};

enum kita_buf_type {
	KITA_BUF_NONE = _IONBF,  // 0x0004
	KITA_BUF_LINE = _IOLBF,  // 0x0040
	KITA_BUF_FULL = _IOFBF   // 0x0000
};

enum kita_evt_type {
	KITA_EVT_CHILD_OPENED,   // child was opened TODO not sure we need this
	KITA_EVT_CHILD_CLOSED,   // child was closed 
	KITA_EVT_CHILD_REAPED,   // child was reaped
	KITA_EVT_CHILD_HANGUP,   // child has hung up TODO this and 'EXITED' are kinda same?
	KITA_EVT_CHILD_EXITED,   // child has exited  TODO this and 'HANGUP' are kinda same?
	KITA_EVT_CHILD_FEEDOK,   // child is ready to be fed data
	KITA_EVT_CHILD_READOK,   // child has data available to read
	KITA_EVT_CHILD_REMOVE,   // child is about to be removed from state
	KITA_EVT_CHILD_ERROR,    // an error occurred
	KITA_EVT_COUNT
};

enum kita_opt_type {
	KITA_OPT_AUTOCLEAN,      // automatically remove reaped children?
	KITA_OPT_AUTOTERM,       // automatically terminate fully closed children?
	KITA_OPT_LAST_LINE,      // only read last line, if multiple lines available
	KITA_OPT_NO_NEWLINE,     // remove '\n' from the end of data, if reading lines
	KITA_OPT_COUNT
};

typedef enum kita_ios_type kita_ios_type_e;
typedef enum kita_buf_type kita_buf_type_e;
typedef enum kita_evt_type kita_evt_type_e;
typedef enum kita_opt_type kita_opt_type_e;

//
// STRUCTS 
//

struct kita_state;
struct kita_child;
struct kita_event;
struct kita_calls;
struct kita_stream;

typedef struct kita_state kita_state_s;
typedef struct kita_child kita_child_s;
typedef struct kita_event kita_event_s;
typedef struct kita_calls kita_calls_s;
typedef struct kita_stream kita_stream_s;

typedef void (*kita_call_c)(kita_state_s* s, kita_event_s* e);

struct kita_stream
{
	FILE* fp;
	int   fd;

	kita_ios_type_e ios_type;
	kita_buf_type_e buf_type;
	unsigned registered : 1;  // child registered with epoll? TODO do we need this?
};

struct kita_child
{
	char* cmd;               // command/binary to run (could have arguments)
	char* arg;               // additional argument string (optional)
	pid_t pid;               // process ID

	kita_stream_s* io[3];    // stream objects for stdin, stdout, stderr
	int status;              // status returned by waitpid(), if any

	kita_state_s* state;     // tracking state, if any

	void* ctx;               // user data
};

struct kita_event
{
	kita_child_s* child;     // associated child process
	kita_evt_type_e type;    // event type
	kita_ios_type_e ios;     // stdin, stdout, stderr?
	int fd;                  // file descriptor for the relevant child's stream
	int size;                // number of bytes available for reading
};

struct kita_state
{
	kita_child_s** children; // child processes
	size_t num_children;     // num of child processes

	kita_call_c cbs[KITA_EVT_COUNT]; // event callbacks

	int epfd;                // epoll file descriptor
	sigset_t sigset;         // signals to be ignored by epoll_wait
	int error;               // last error that occured
	unsigned char options[KITA_OPT_COUNT]; // boolean options

	void* ctx;               // user data ('context')
};

//
// FUNCTIONS
//

// Initialization
kita_state_s* kita_init();
int kita_set_callback(kita_state_s* s, kita_evt_type_e type, kita_call_c cb);

// Main flow control
int kita_loop(kita_state_s* s);
int kita_tick(kita_state_s* s, int timeout);

// Children: creating, deleting, registering
kita_child_s* kita_child_new(const char* cmd, int in, int out, int err);
int           kita_child_add(kita_state_s* s, kita_child_s* c);
int           kita_child_del(kita_state_s* s, kita_child_s* c);

void kita_child_free(kita_child_s** c); // TODO ?

// Children: setting and getting options
int           kita_child_set_buf_type(kita_child_s* c, kita_ios_type_e ios, kita_buf_type_e buf);
void          kita_child_set_context(kita_child_s* c, void *ctx);
void*         kita_child_get_context(kita_child_s* c);
void          kita_child_set_arg(kita_child_s* c, char* arg);
char*         kita_child_get_arg(kita_child_s* c);
kita_state_s* kita_child_get_state(kita_child_s* c);

// Children: opening, reading, writing, killing
int   kita_child_feed(kita_child_s* c, const char* str);
char* kita_child_read(kita_child_s* c, kita_ios_type_e n);
int   kita_child_open(kita_child_s* c);
int   kita_child_close(kita_child_s* c); 
int   kita_child_reap(kita_child_s* c);
int   kita_child_kill(kita_child_s* c);
int   kita_child_term(kita_child_s* c);

// Children: inquire, status
int kita_child_is_open(kita_child_s* c);
int kita_child_is_alive(kita_child_s* c);

// Clean-up and shut-down
void kita_kill(kita_state_s* s);
void kita_free(kita_state_s** s);

void kita_set_option(kita_state_s* s, kita_opt_type_e opt, unsigned char val);
char kita_get_option(kita_state_s* s, kita_opt_type_e opt);

// Custom user-data
void  kita_set_context(kita_state_s* s, void *ctx);
void* kita_get_context(kita_state_s* s);

// Retrieval of data from the twirc state TODO
//int kita_get_last_error(const kita_state_s* s);

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  IMPLEMENTATION                                                            //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifdef KITA_IMPLEMENTATION

#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <unistd.h>    // pipe(), fork(), dup(), close(), _exit(), ...
#include <string.h>    // strlen()
#include <errno.h>     // errno
#include <fcntl.h>     // fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <spawn.h>     // posix_spawnp()
#include <wordexp.h>   // wordexp(), wordfree(), ...
#include <sys/epoll.h> // epoll_create, epoll_wait(), ... 
#include <sys/types.h> // pid_t
#include <sys/wait.h>  // waitpid()
#include <sys/ioctl.h> // ioctl(), FIONREAD
#include "libkita.h"

static volatile int running;   // Main loop control 
extern char **environ;         // Required to pass the environment to children

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  PRIVATE FUNCTIONS                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

static int
libkita_empty(const char *str)
{
	return (str == NULL || str[0] == '\0');
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
static pid_t
libkita_popen(const char *cmd, FILE **in, FILE **out, FILE **err)
{
	if (!cmd || !strlen(cmd))
	{
		return -1;
	}

	// 0 = read end of pipes, 1 = write end of pipes
	int pipe_stdin[2];
	int pipe_stdout[2];
	int pipe_stderr[2];

	if (in && (pipe(pipe_stdin) < 0))
	{
		return -1;
	}
	if (out && (pipe(pipe_stdout) < 0))
	{
		return -1;
	}
	if (err && (pipe(pipe_stderr) < 0))
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
		// redirect stdin to the read end of this pipe
		if (in)
		{
			if (dup2(pipe_stdin[0], STDIN_FILENO) == -1)
			{
				_exit(-1);
			}
			close(pipe_stdin[1]); // child doesn't need write end
		}
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
		if (in)
		{
			close(pipe_stdin[0]);  // parent doesn't need read end
			*in = fdopen(pipe_stdin[1], "w");
		}
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
		return pid;
	}
}

/*
 * Examines the given file descriptor for the number of bytes available for 
 * reading and returns that number. On error, -1 will be returned.
 */
static int
libkita_fd_data_avail(int fd)
{
	int bytes = 0;
	return ioctl(fd, FIONREAD, &bytes) == -1 ? -1 : bytes;
}

static int
libkita_child_has_fd(kita_child_s *child, int fd)
{
	for (int i = 0; i < 3; ++i)
	{
		if (child->io[i] && child->io[i]->fd == fd)
		{
			return 1;
		}
	}
	return 0;
}

/*
 * Finds and returns the child with the given `pid` or NULL.
 */
static kita_child_s*
libkita_child_get_by_pid(kita_state_s *state, pid_t pid)
{
	for (size_t i = 0; i < state->num_children; ++i)
	{	
		if (state->children[i]->pid == pid)
		{
			return state->children[i];
		}
	}
	return NULL;
}

static kita_child_s*
libkita_child_get_by_fd(kita_state_s *state, int fd)
{
	for (size_t i = 0; i < state->num_children; ++i)
	{
		if (libkita_child_has_fd(state->children[i], fd))
		{
			return state->children[i];
		}

	}
	return NULL;
}

/*
 * Find the index (array position) of the given child.
 * Returns the index position or -1 if no such child found.
 */
static int
libkita_child_get_idx(kita_state_s *state, kita_child_s *child)
{
	for (size_t i = 0; i < state->num_children; ++i)
	{
		if (state->children[i] == child)
		{
			return i;
		}
	}
	return -1;
}

static kita_ios_type_e
libkita_child_fd_get_type(kita_child_s *child, int fd)
{
	for (int i = 0; i < 3; ++i)
	{
		if (child->io[i] && child->io[i]->fd == fd)
		{
			return (kita_ios_type_e) i;
		}
	}
	return -1;
}

static int
libkita_stream_set_buf_type(kita_stream_s *stream, kita_buf_type_e buf)
{
	// From the setbuf manpage:
	// > The setvbuf() function may be used only after opening a stream
	// > and before any other operations have been performed on it.

	if (stream->fp == NULL) // can't modify if not yet open
	{
		return -1;
	}
	
	if (setvbuf(stream->fp, NULL, buf, 0) != 0)
	{
		return -1;
	}

	stream->buf_type = buf;
	return 0;
}

/*
 * Register the given stream's file descriptor with the state's epoll instance.
 */
static int
libkita_stream_reg_ev(kita_state_s *state, kita_stream_s *stream)
{
	if (stream->fp == NULL) // we don't register a closed stream
	{
		return -1;
	}

	int fd = fileno(stream->fp);
	int ev = stream->ios_type == KITA_IOS_IN ? EPOLLOUT : EPOLLIN;

	struct epoll_event epev = { .events = ev | EPOLLET, .data.fd = fd };
	
	if (epoll_ctl(state->epfd, EPOLL_CTL_ADD, fd, &epev) == 0)
	{
		stream->registered = 1;
		return 0;
	}
	return -1;
}

/*
 * Remove the given stream's file descriptor from the state's epoll instance.
 */
static int
libkita_stream_rem_ev(kita_state_s *state, kita_stream_s *stream)
{
	if (epoll_ctl(state->epfd, EPOLL_CTL_DEL, stream->fd, NULL) == 0)
	{
		stream->registered = 0;
		return 0;
	}
	return -1;
}

/*
 * Register all events for the given child with the epoll file descriptor.
 * Returns the number of events registered.
 */
static int 
libkita_child_reg_events(kita_state_s *state, kita_child_s *child)
{
	int reg = 0;
	for (int i = 0; i < 3; ++i)
	{
		if (child->io[i])
		{
			reg += libkita_stream_reg_ev(state, child->io[i]) == 0;
		}
	}
	return reg;
}

/*
 * Removes all events for the given child from the epoll file descriptor.
 * Returns the number of events deleted.
 */
static int
libkita_child_rem_events(kita_state_s *state, kita_child_s *child)
{
	int rem = 0;
	for (int i = 0; i < 3; ++i)
	{
		if (child->io[i])
		{
			rem += libkita_stream_rem_ev(state, child->io[i]) == 0;
		}
	}
	return rem;
}

/*
 * Closes the given stream via fclose().
 * Returns 0 on success, -1 if the stream wasn't open in the first place.
 */
static int
libkita_stream_close(kita_stream_s *stream)
{
	if (stream->fp == NULL)
	{
		stream->fd = -1;
		return -1;
	}

	fclose(stream->fp);
	stream->fp = NULL;
	stream->fd = -1;
	return 0;
}

/*
 * Create a kita_stream_s struct on the heap (malloc'd) and 
 * initialize it to the given stream type `ios` and buffer type `buf`. 
 * Returns the allocated structure or NULL if out of memory.
 */
static kita_stream_s*
libkita_stream_new(kita_ios_type_e ios)
{
	kita_stream_s *stream = malloc(sizeof(kita_stream_s));
	if (stream == NULL)
	{
		return NULL;
	}
	*stream = (kita_stream_s) { 0 };

	// file descriptor
	stream->fd = -1;
	
	// set stream type and stream buffer type
	stream->ios_type = ios;
	stream->buf_type = (ios == KITA_IOS_ERR) ? KITA_BUF_NONE : KITA_BUF_LINE;

	return stream;
}

/*
 * Set the blocking behavior of the given stream, where 0 means non-blocking 
 * and 1 means blocking. Returns 0 on success, -1 on error.
 */
int
libkita_stream_set_blocking(kita_stream_s *stream, int blocking)
{
	if (stream->fp == NULL) // can't modify if not yet open
	{
		return -1;
	}

	if (stream->fd < 2) // can't modify without valid file descriptor
	{
		return -1;
	}

	//int fd = fileno(stream->fp);
	int flags = fcntl(stream->fd, F_GETFL, 0);

	if (flags == -1)
	{
		return -1;
	}
	if (blocking) // make blocking
	{
	  	flags &= ~O_NONBLOCK;
	}
	else          // make non-blocking
	{
		flags |=  O_NONBLOCK;
	}
	if (fcntl(stream->fd, F_SETFL, flags) != 0)
	{
		return -1;
	}

	return 0;
}

static int
libkita_child_open(kita_child_s *child)
{
	if (child->pid > 0) 
	{
		// ALREADY OPEN
		return -1;
	}

	if (libkita_empty(child->cmd))
	{
		// NO COMMAND GIVEN
		return -1;
	}

	// Construct the command, if there is an additional argument string
	char *cmd = NULL;
	if (child->arg)
	{
		size_t len = strlen(child->cmd) + strlen(child->arg) + 4;
		cmd = malloc(sizeof(char) * len);
		snprintf(cmd, len, "%s %s", child->cmd, child->arg);
	}
	
	// Execute the block and retrieve its PID
	child->pid = libkita_popen(
			cmd ? cmd : child->cmd, 
			child->io[KITA_IOS_IN]  ? &child->io[KITA_IOS_IN]->fp  : NULL,
			child->io[KITA_IOS_OUT] ? &child->io[KITA_IOS_OUT]->fp : NULL,
		        child->io[KITA_IOS_ERR] ? &child->io[KITA_IOS_ERR]->fp : NULL);
	free(cmd);

	// Check if that worked
	if (child->pid == -1)
	{
		// popen_noshell() failed to open it
		return -1;
	}
	
	// Get file descriptors from open file pointers
	// and make sure the streams have the correct buffer type
	for (int i = 0; i < 3; ++i)
	{
		if (child->io[i] && child->io[i]->fp)
		{
			child->io[i]->fd = fileno(child->io[i]->fp);
			libkita_stream_set_buf_type(child->io[i], child->io[i]->buf_type);
			// TODO wouldn't it be best to set non-blocking right here?
			//libkita_stream_set_blocking(child->io[i], 0);
		}
	}
	
	return 0;
}

/*
 * Close the child's file pointers via fclose(), then set them to NULL.
 * Returns the number of file pointers that have been closed.
 */
static int
libkita_child_close(kita_child_s *child)
{
	int num_closed = 0;
	for (int i = 0; i < 3; ++i)
	{
		if (child->io[i] != NULL)
		{
			num_closed += (libkita_stream_close(child->io[i]) == 0);
		}
	}
	return num_closed;
}

static size_t
libkita_child_add(kita_state_s *state, kita_child_s *child)
{
	// array index for the new child
	int idx = state->num_children++;

	// increase array size
	size_t new_size = state->num_children * sizeof(state->children);
	kita_child_s** children = realloc(state->children, new_size);
	if (children == NULL)
	{
		return --state->num_children;
	}
	state->children = children;

	// add new child
	state->children[idx] = child;

	// mark new child as tracked
	state->children[idx]->state = state;

	// return new number of children
	return state->num_children;
}

/*
 * Removes this child from the state. The child will not be stopped or closed,
 * nor will its events be deleted from the state's epoll instance. 
 * Returns the new number of children tracked by the state.
 */
static size_t
libkita_child_del(kita_state_s *state, kita_child_s *child)
{
	// find the array index of the given child
	int idx = libkita_child_get_idx(state, child);
	if (idx < 0)
	{
		// child not found, do nothing
		return state->num_children;
	}

	// remove state reference from child
	child->state = NULL;

	// reduce child counter by one
	--state->num_children;

	// copy the ptr to the last element into this element
	if (state->num_children != idx)
	{
		state->children[idx] = state->children[state->num_children];
		state->children[state->num_children] = NULL;
	}

	// figure out the new size
	size_t new_size = state->num_children * sizeof(state->children);

	// if we deleted the last element, just use free() and return
	if (new_size == 0)
	{
		free(state->children);
		return 0;
	}

	// realloc() the array to the new size
	kita_child_s** children = realloc(state->children, new_size);
	
	// in case realloc failed...
	if (children == NULL)
	{
		// realloc() failed, which means we're stuck with the old 
		// memory, which is too large by one element and now has 
		// a duplicate element (the last one and the one at `idx`),
		// which is annoying, but if we pretend the size to be one 
		// smaller than it actually is, then we should never find 
		// ourselves accidentally trying to access that last element;
		// and hopefully the next realloc() will succeed and fix it.
		return state->num_children; 
	}

	state->children = children;
	return state->num_children;
}

/*
 * Init the epoll instance for the given state.
 * Returns 0 on success, -1 on error.
 */
static int
libkita_init_epoll(kita_state_s *state)
{
	int epfd = epoll_create(1);
	if (epfd < 0)
	{
		return -1;
	}
	state->epfd = epfd;
	return 0;
}

static int
libkita_dispatch_event(kita_state_s *state, kita_event_s *event)
{
	if (state->cbs[event->type] == NULL)
	{
		return -1;
	}
	state->cbs[event->type](state, event);
	return 0;
}

/*
 * Uses waitid() to figure out if the given child is alive or dead.
 * Returns 1 if child is alive, 0 if dead, -1 if status is unknown.
 */ 
static int
libkita_child_status(kita_child_s *child)
{
	siginfo_t info = { 0 };
	int options = WEXITED | WSTOPPED | WNOHANG | WNOWAIT;
	if (waitid(P_PID, child->pid, &info , options) == -1)
	{
		// waitid() error, status unknown
		return -1;
	}
	if (info.si_pid == 0) 
	{
		// no state change for PID, still running
 		return 1;
	}
	// we don't need to inspect info.si_code, because
	// we only wait for exited/stopped children anyway
	return 0;
}

/*
 * Uses waitpid() to identify children that have died. Dead children will be 
 * closed (by closing all of their streams) and their PID will be reset to 0. 
 * The REAPED event will be dispatched for each child reaped this way.
 * Returns the number of reaped children.
 */
static int
libkita_reap(kita_state_s *state)
{
	// waitpid() with WNOHANG will return:
	//  - PID of the child that has changed state, if any
	//  -  0  if there are relevant children, but none have changed state
	//  - -1  on error

	int reaped = 0;
	pid_t pid  = 0;
	int status = 0;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		kita_child_s *child = libkita_child_get_by_pid(state, pid);
		if (child)
		{
			// remember the child's waitpid status
			child->status = status;

			// remove epoll events
			libkita_child_rem_events(state, child);

			// close the child's streams
			libkita_child_close(child); 

			// prepare the event struct
			kita_event_s event = { 0 };
			event.child = child;
			event.ios   = KITA_IOS_ALL;

			// dispatch close event
			event.type  = KITA_EVT_CHILD_CLOSED;
			libkita_dispatch_event(state, &event);

			// dispatch reap event
			event.type  = KITA_EVT_CHILD_REAPED;
			libkita_dispatch_event(state, &event);

			// finally, set the PID to 0
			child->pid = 0;

			++reaped;
		}
	}
	return reaped;
}

/*
 * Inspects all children, removing all of those that have been manually reaped 
 * by user code (indicated by their PID being 0), also removing their events. 
 */
static size_t
libkita_autoclean(kita_state_s *state)
{
	for (size_t i = 0; i < state->num_children; ++i)
	{
		if (state->children[i]->pid == 0)
		{
			// TODO
			// we need to send the REMOVE event _before_ we actually 
			// remove the child from the state, otherwise we would 
			// not be able to add a reference to the child to the 
			// event struct (it would be NULL!), hence the user 
			// would not know which child was removed. this means
			// that it is more of a "WILL_BE_REMOVED" event rather
			// than as "HAS_BEEN_REMOVED" event. gotta document that
			// very clearly somehow! ... or is there a better way?

			kita_event_s ev = { 0 };
			ev.child = state->children[i];
			ev.type  = KITA_EVT_CHILD_REMOVE;
			ev.ios   = KITA_IOS_NONE;
			ev.fd    = -1;
			libkita_dispatch_event(state, &ev);

			// remove child from epoll
			libkita_child_rem_events(state, state->children[i]);

			// remove child from state
			libkita_child_del(state, state->children[i]);
		}
	}
	return state->num_children;
}

/*
 * Inspects all children, terminating those that have been fully closed, 
 * possibly by user code (indicated by all of their streams being NULL).
 */
static int
libkita_autoterm(kita_state_s *state)
{
	int terminated = 0;
	for (size_t i = 0; i < state->num_children; ++i)
	{
		if (kita_child_is_open(state->children[i]) == 0)
		{
			terminated += (kita_child_term(state->children[i]) == 0);
		}
	}
	return terminated;
}

static int
libkita_handle_event(kita_state_s *state, struct epoll_event *epev)
{
	kita_child_s *child = libkita_child_get_by_fd(state, epev->data.fd);
	if (child == NULL)
	{
		return 0;
	}

	kita_event_s event = { 0 };
	event.child = child;
	event.fd    = epev->data.fd; 
	event.ios   = libkita_child_fd_get_type(child, epev->data.fd);

	// EPOLLIN: We've got data coming in
	if(epev->events & EPOLLIN)
	{
		event.type = KITA_EVT_CHILD_READOK; 
		event.size = libkita_fd_data_avail(event.fd);
		libkita_dispatch_event(state, &event);
		return 0;
	}
	
	// EPOLLOUT: We're ready to send data
	if (epev->events & EPOLLOUT)
	{
		event.type = KITA_EVT_CHILD_FEEDOK;
		libkita_dispatch_event(state, &event);
		return 0;
	}
	
	// EPOLLRDHUP: Server closed the connection
	// EPOLLHUP:   Unexpected hangup on socket 
	if (epev->events & EPOLLRDHUP || epev->events & EPOLLHUP)
	{
		// dispatch hangup event
		event.type = KITA_EVT_CHILD_HANGUP;
		libkita_dispatch_event(state, &event);

		// close the stream
		libkita_stream_rem_ev(state, child->io[event.ios]);
		libkita_stream_close(child->io[event.ios]);

		// create closed event by making a copy of the original
		kita_event_s event_closed = event;
		event_closed.type = KITA_EVT_CHILD_CLOSED;
		
		// dispatch closed event
		libkita_dispatch_event(state, &event_closed);
		return 0;
	}
	
	// EPOLLERR: Error on file descriptor (could also mean: stdin closed)
	if (epev->events & EPOLLERR) // fires even if not added explicitly
	{
		event.type = KITA_EVT_CHILD_ERROR;
		libkita_dispatch_event(state, &event);
	
		// event happened on stdin: stdin was probably closed
		// EBADF is set: file descriptor is not valid (anymore)
		if (event.ios == KITA_IOS_IN || errno == EBADF) 
		{
			libkita_stream_rem_ev(state, child->io[event.ios]);
			libkita_stream_close(child->io[event.ios]);

			// dispatch closed event
			event.type = KITA_EVT_CHILD_CLOSED;
			libkita_dispatch_event(state, &event);
		}
		return 0;
	}
	
	// Handled everything and no error occurred
	return 0;
}


/*
 * Closes the given stream, then frees its memory and sets it to NULL. 
 */
void
libkita_stream_free(kita_stream_s** stream)
{
	if ((*stream)->fp)
	{
		libkita_stream_close(*stream);
	}

	free(*stream);
	*stream = NULL;
}

/*
 * TODO - currently we only ever get the last line, regardles of `last`
 *      - error handling for getline() (EOF, error) 
 */
static char*
libkita_stream_read_line(kita_stream_s *stream, int last, int no_nl)
{
	if (stream->fp == NULL)
	{
		return NULL;
	}

	// TODO I thought getline() would be the perfect tool here, as it even 
	//      allocates a suitable buffer by itself. however, for whatever 
	//      reason, getline() returns -1 with EAGAIN _after_ the first line 
	//      (for the second, third, ... line) - why does it fail to read?! 
	//      data keeps building up...
	/*
	  [...] getline() approach here
	*/

	// TODO there is a bug in here. imagine this scenario: despite being
	//      line buffered, the file descriptor has _multiple_ lines ready 
	//      to be read (yes, this does happen). in this case, while we do 
	//      seem to get the correct amount of data waiting, we fail to 
	//      read all of it, because fgets() stops at a newline - and there 
	//      are now two (or more) newlines in the data to be read.
	/*
	size_t len = libkita_fd_data_avail(stream->fd) + 1;
	char*  buf = malloc(len * sizeof(char));

	fgets(buf, len, stream->fp);
	*/

	size_t num_lines = 0;
	size_t len = libkita_fd_data_avail(stream->fd) + 2;
	char*  buf = malloc(len * sizeof(char));
	
	// fgets() - reads until a newline ('\n') or EOF (end of file)
	//         - returns NULL on error or when EOF occurs
	while (fgets(buf, len, stream->fp) != NULL)
	{
		++num_lines;
	}

	// remove trailing newline, if requested
	if (no_nl)
	{
		buf[strcspn(buf, "\n")] = 0;
	}

	// return the last line we read
	return buf;
}

/*
 * TODO - what if libkita_fd_data_avail() comes back as -1 (can't determine)?
 *      - what if fread() encounters EOF (feof()) or an error (ferror())?
 */
static char*
libkita_stream_read_data(kita_stream_s *stream)
{
	if (stream->fd < 2)
	{
		return NULL;
	}
	if (stream->fp == NULL)
	{
		return NULL;
	}

	// TODO I believe there to be a bug here (race condition), where there
	//      could be new data already arriving between the calls to 
	//      libkita_fd_data_avail() and fgets(), effectively making us 
	//      not read all the data that is available, which then gets us 
	//      into some unholy stuck mess!
	size_t len = libkita_fd_data_avail(stream->fd) + 2;
	char*  buf = malloc(len * sizeof(char));

	fread(buf, len, 1, stream->fp);
	return buf;
}

// TODO implement (properly)
static char*
libkita_stream_read(kita_stream_s *stream, int last, int no_nl)
{
	if (stream->buf_type == KITA_BUF_LINE)
	{
		return libkita_stream_read_line(stream, last, no_nl);
	}

	else
	{
		return libkita_stream_read_data(stream);
	}
}

int
libkita_poll(kita_state_s *s, int timeout)
{
	struct epoll_event epev;
	
	// epoll_wait()/epoll_pwait() will return -1 if a signal is caught.
	// User code might catch "harmless" signals, like SIGWINCH, that are
	// ignored by default. This would then cause epoll_wait() to return
	// with -1, hence our main loop to come to a halt. This is not what
	// a user would expect; we should only come to a halt on "serious"
	// signals that would cause program termination/halt by default.
	// In order to achieve this, we tell epoll_pwait() to block all of
	// the signals that are ignored by default. For a list of signals:
	// https://en.wikipedia.org/wiki/Signal_(IPC)
	
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);  // default: ignore
	sigaddset(&sigset, SIGCONT);  // default: continue execution
	sigaddset(&sigset, SIGURG);   // default: ignore
	sigaddset(&sigset, SIGWINCH); // default: ignore

	// timeout = -1 -> block indefinitely, until events available
	// timeout =  0 -> return immediately, even if no events available
	int num_events = epoll_pwait(s->epfd, &epev, 1, timeout, &sigset);

	// An error has occured
	if (num_events == -1)
	{
		// The exact reason why epoll_wait failed can be queried through
		// errno; the possibilities include wrong/faulty parameters and,
		// more interesting, that a signal has interrupted epoll_wait().
		// Wrong parameters will either happen on the very first call or
		// not at all, but a signal could come in anytime. Some signals, 
		// like SIGSTOP, can mean that we're simply supposed to stop 
		// execution until a SIGCONT is received. Hence, it seems like 
		// a good idea to leave it up to the user what to do, which 
		// means that we might want to return -1 to indicate an issue. 
		// The user can then check errno and decide if they want to 
		// keep going / start again or stop for good.
		//
		// Set the error accordingly:
		//  - KITA_ERR_EPOLL_SIG  if epoll_pwait() caught a signal
		//  - KITA_ERR_EPOLL_WAIT for any other error in epoll_wait()
		s->error = errno == EINTR ? KITA_ERR_EPOLL_SIG : KITA_ERR_EPOLL_WAIT;
		
		return -1;
	}

	libkita_handle_event(s, &epev); // TODO what to do with the return val?
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  PUBLIC FUNCTIONS                                                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/*
 * Returns the number of open streams for this child or 0 if none are open.
 */
int 
kita_child_is_open(kita_child_s *child)
{
	int open = 0;
	for (int i = 0; i < 3; ++i)
	{
		if (child->io[i])
		{
			open += child->io[i]->fp != NULL;
		}
	}
	return open;
}

/*
 * Returns 1 if the child is still alive, 0 otherwise.
 */
int
kita_child_is_alive(kita_child_s *child)
{
	// PID of 0 means the child is dead or was never alive
	if (child->pid == 0)
	{
		return 0;
	}
	
	// TODO this can return -1 if waitid() failed, in which
	//      case we don't know if child is dead or alive...
	return libkita_child_status(child) == 1;
}

/*
 * Uses waitpid() to check if the child has terminated. If so, the child will 
 * be closed (by closing all of its streams) and its PID will be reset to 0. 
 * Returns the PID of the reaped child, 0 if the child wasn't reaped or -1 if 
 * the call to waitpid() encountered an error (inspect errno for details).
 * Note: this function is for children that are _untracked_ (have not been 
 *       added to a state); it will do nothing if the given child is tracked. 
 *       Also, no events (neither CLOSED nor REAPED) will be dispatched.
 */
int 
kita_child_reap(kita_child_s *child)
{
	// tracked children will be reaped automatically, abort 
	if (child->state)
	{
		return -1;
	}

	int pid = waitpid(child->pid, &child->status, WNOHANG);
	if (child->pid == pid)
	{
		// close the child's streams
		libkita_child_close(child); 

		// finally, set the PID to 0
		child->pid = 0;
	}
	return pid;
}

int
kita_child_skip(kita_child_s *child, kita_ios_type_e ios)
{
	if (ios == KITA_IOS_IN)         // can't seek stdin
	{
		return -1;
	}
	if (child->io[ios] == NULL)     // no such stream
	{
		return -1;
	}
	if (child->io[ios]->fp == NULL) // stream closed
	{
		return -1;
	}
	return fseek(child->io[ios]->fp, 0, SEEK_END);
}

/*
 * Closes all open streams of this child. If the child is tracked by a state, 
 * all events for the child's streams will also be removed. Note that closing 
 * the child will not automatically terminate it, however. Closing the child 
 * will merely close down all communication channels to and from the child; 
 * the child will continue to run; you should still receive a signal once the 
 * child terminates. If you want to stop the child, kill or terminate it.
 * Returns 0 on success, -1 on error.
 */
int
kita_child_close(kita_child_s *child)
{
	// if child is tracked, unregister its events 
	if (child->state)
	{
		libkita_child_rem_events(child->state, child);
	}

	// close all streams
	return libkita_child_close(child) > 0 ? 0 : -1;
}

/*
 * Set the child's stream, specified by `ios`, to the buffer type specified
 * via `buf`. Returns 0 on success, -1 on error.
 */
int
kita_child_set_buf_type(kita_child_s *child, kita_ios_type_e ios, kita_buf_type_e buf)
{
	if (child->io[ios] == NULL)
	{
		return -1;
	}
	return libkita_stream_set_buf_type(child->io[ios], buf);
}

/*
 * Get the buffer type of the child's stream specified by `ios`.
 * Returns the buffer type or -1 if there is no such stream.
 */
kita_buf_type_e
kita_child_get_buf_type(kita_child_s *child, kita_ios_type_e ios)
{
	if (child->io[ios] == NULL)
	{
		return -1;
	}
	
	return child->io[ios]->buf_type;
}

/*
 * Save a reference to `arg`, which will be used as additional argument 
 * when opening or running this child. Use `NULL` to clear the argument.
 */
void
kita_child_set_arg(kita_child_s *child, char *arg)
{
	child->arg = arg;
}

char*
kita_child_get_arg(kita_child_s *child)
{
	return child->arg;
}

void
kita_child_set_context(kita_child_s *child, void *ctx)
{
	child->ctx = ctx;
}

void*
kita_child_get_context(kita_child_s *child)
{
	return child->ctx;
}

kita_state_s*
kita_child_get_state(kita_child_s *child)
{
	return child->state;
}

void
kita_set_context(kita_state_s *state, void *ctx)
{
	state->ctx = ctx;
}

void*
kita_get_context(kita_state_s *state)
{
	return state->ctx;
}

/*
 * Opens (runs) the given child. If the child is tracked by the state, events 
 * for all opened streams will automatically be registered as well.
 * Returns 0 on success, -1 on error.
 */
int
kita_child_open(kita_child_s *child)
{
	int open = libkita_child_open(child);
	
	// if opening failed, return error code
	if (open < 0)
	{
		return open;
	}
	
	// make stdout and stderr streams non-blocking
	if (child->io[KITA_IOS_OUT])
	{
		libkita_stream_set_blocking(child->io[KITA_IOS_OUT], 0);
	}
	if (child->io[KITA_IOS_ERR])
	{
		libkita_stream_set_blocking(child->io[KITA_IOS_ERR], 0);
	}

	// if child is tracked, register events for it
	if (child->state)
	{
		libkita_child_reg_events(child->state, child);
	}
	return 0;
}

/*
 * Sends the SIGKILL signal to the child. SIGKILL can not be ignored 
 * and leads to immediate shut-down of the child process, no clean-up.
 * Returns 0 on success, -1 on error.
 */
int
kita_child_kill(kita_child_s *child)
{
	if (child->pid < 2)
	{
		return -1;
	}
	
	// We do not set the child's PID to 0 here, because it seems
	// like the better approach to detect all child deaths via 
	// waitpid() or some other means (same approach for all).
	return kill(child->pid, SIGKILL);
}

/*
 * Sends the SIGTERM signal to the child. SIGTERM can be ignored or 
 * handled and allows the child to do clean-up before shutting down.
 * Returns 0 on success, -1 on error.
 */
int
kita_child_term(kita_child_s *child)
{
	if (child->pid < 2)
	{
		return -1;
	}
	
	// We do not set the child's PID to 0 here, because the 
	// child might not immediately terminate (clean-up, etc). 
	// Instead, we should catch SIGCHLD, then use waitpid()
	// to determine the termination and to set PID to 0.
	return kill(child->pid, SIGTERM);
}

/*
 * Attempts to read from the child's stream specified by `ios` (should be one 
 * of KITA_IOS_OUT, KITA_IOS_ERR) and returns the read bytes as a dynamically
 * allocated string (caller needs to free it at some point). All available data 
 * will be read. For line buffered streams, this means that all lines will be 
 * read, if multiple are available. If the child is tracked by the state and 
 * the LAST_LINE option is enabled, all but the last line will be discarded. 
 * If the NO_NEWLINE option is enabled in addition to LAST_LINE, the line feed 
 * (new line) character '\n' will be removed from the returned buffer.
 * Returns NULL on error or if there was no data available for reading.
 */
char*
kita_child_read(kita_child_s *child, kita_ios_type_e ios)
{
	if (ios != KITA_IOS_OUT && ios != KITA_IOS_ERR)
	{
		return NULL;
	}

	if (child->io[ios] == NULL) // no such stream
	{
		return NULL;
	}

	// TODO - would it be nicer if we just allocated a buffer internally?
	//      - maybe use getline() instead? It allocates a suitable buffer!
	//      - maybe we can put this to use: libkita_fd_data_avail(int fd)
	//      - implement differently depending on the child's buffer type?
	//      - fgets() vs getline() vs read() vs fread() vs ... !?
	// https://stackoverflow.com/questions/6220093/
	// https://stackoverflow.com/questions/2751632/
	// https://stackoverflow.com/questions/584142/
	// -- I think we should use fread() for fully or unbuffered streams,
	//    and fgets() or getline() for line buffered streams

	kita_state_s* state = child->state;
	int last = state ? kita_get_option(state, KITA_OPT_LAST_LINE) : 0;
	int nonl = state ? kita_get_option(state, KITA_OPT_NO_NEWLINE) : 0;

	return libkita_stream_read(child->io[ios], last, nonl);
}

/*
 * Writes the given `input` to the child's stdin stream.
 * Returns 0 on success, -1 on error.
 */
int
kita_child_feed(kita_child_s *child, const char *input)
{
	// child doesn't have a stdin stream
	if (child->io[KITA_IOS_IN] == NULL)
	{
		return -1;
	}
	
	// child's stdin file pointer isn't open
	if (child->io[KITA_IOS_IN]->fp == NULL) 
	{
		return -1;
	}

	// no input given, or input is empty 
	if (libkita_empty(input))
	{
		return -1;
	}

	return (fputs(input, child->io[KITA_IOS_IN]->fp) == EOF) ? -1 : 0;
}

void
kita_child_free(kita_child_s** child)
{
	// this is necessary because kita_child_del() will change the address 
	// of the pointer, hence we can't dereference `child` after the call,
	// but a copy of it (here: `c`) will have a different address, solved
	kita_child_s* c = *child;

	// unregister events and delete from state
	if (c->state)
	{
		kita_child_del(c->state, c);
	}

	// send SIGKILL if child is still running
	//kita_child_kill(c);

	// free the child's cmd string
	free(c->cmd);

	// free the streams (this also closes them)
	for (int i = 0; i < 3; ++i)
	{
		if (c->io[i])
		{
			libkita_stream_free(&c->io[i]);
		}
	}

	// finally free the child struct itself
	free(c);
	c = NULL;
}

/*
 * Dynamically allocates a kita child and returns a pointer to it.
 * Returns NULL in case malloc() failed (out of memory).
 */
kita_child_s*
kita_child_new(const char *cmd, int in, int out, int err)
{
	kita_child_s *child = malloc(sizeof(kita_child_s));
	if (child == NULL)
	{
		return NULL;
	}

	// zero-initialize
	*child = (kita_child_s) { 0 };

	// copy the command
	child->cmd = strdup(cmd);

	// create input/output streams as requested
	child->io[KITA_IOS_IN]  = in ? 	libkita_stream_new(KITA_IOS_IN)  : NULL;
	child->io[KITA_IOS_OUT] = out ?	libkita_stream_new(KITA_IOS_OUT) : NULL;
	child->io[KITA_IOS_ERR] = err ?	libkita_stream_new(KITA_IOS_ERR) : NULL;
	
	return child;
}

/*
 * TODO documentation ...
 * Returns 0 on success, -1 on error.
 */
int
kita_child_add(kita_state_s *state, kita_child_s *child)
{
	// child is already tracked (by this or another state)
	if (child->state)
	{
		// TODO set/return error code
		return -1;
	}
	return state->num_children < libkita_child_add(state, child) ? 0 : -1;
}

/*
 * Removes this child from the state. Any registered events will be removed 
 * in the process. However, the child will not be stopped or closed; it is 
 * up to the user to do that before calling this functions.
 * Returns 0 on success, -1 on error.
 */
int
kita_child_del(kita_state_s *state, kita_child_s *child)
{
	// make sure the child is registered with the given state 
	if (child->state != state)
	{
		return -1;
	}
	
	// remove child from epoll
	libkita_child_rem_events(state, child);

	// remove child from state
	return state->num_children > libkita_child_del(state, child) ? 0 : -1;
}

/*
 * Returns the option specified by `opt`, either 0 or 1.
 * If the specified option doesn't exist, -1 is returned.
 */
char
kita_get_option(kita_state_s *state, kita_opt_type_e opt)
{
	// invalid option type
	if (opt < 0 || opt >= KITA_OPT_COUNT)
	{
		return -1; 
	}
	return state->options[opt];
}

/*
 * Sets the option specified by `opt` to `val`, where val is 0 or 1.
 * For any value greater than 1, the option will be set to 1.
 */
void
kita_set_option(kita_state_s *state, kita_opt_type_e opt, unsigned char val)
{
	// invalid option type
	if (opt < 0 || opt >= KITA_OPT_COUNT)
	{
		return;
	}
	state->options[opt] = (val > 0); // limit to [0, 1]
}

int
kita_set_callback(kita_state_s *state, kita_evt_type_e type, kita_call_c cb)
{
	// invalid event type
	if (type < 0 || type >= KITA_EVT_COUNT)
	{
		return -1;
	}
	// set the callback
	state->cbs[type] = cb;
	return 0;
}

int
kita_tick(kita_state_s *state, int timeout)
{
	// wait for child events via epoll_pwait()
	libkita_poll(state, timeout);
	
	// reap dead children via waitpid()
	libkita_reap(state);

	// remove children that terminated without us noticing
	if (state->options[KITA_OPT_AUTOCLEAN])
	{
		libkita_autoclean(state);
	}

	// terminate children that were closed without us noticing
	if (state->options[KITA_OPT_AUTOTERM])
	{
		libkita_autoterm(state);
	}

	return 0; // TODO
}

// TODO - we need some more condition as to when we quit the loop?
//      - make the timeout (-1 hardcoded) a parameter of the function?
//      - also, check the todos within the function
int
kita_loop(kita_state_s* state)
{
	while (kita_tick(state, -1) == 0)
	{
		// Nothing to do here
	}
	// TODO - should we close all children here?
	//      - if so, we should also reap them!
	//      - but this means we'd need another round of epoll_wait?
	fprintf(stderr, "error code = %d\n", state->error);
	return 0; // TODO
}

/*
 * Sends a SIGKILL signal to all children, if any, known by the state.
 */
void 
kita_kill(kita_state_s* state)
{
	// terminate all children
	for (size_t i = 0; i < state->num_children; ++i)
	{
		kita_child_kill(state->children[i]);
	}
}

/*
 * Frees the state, including all children, if any, and sets them to NULL.
 * This does not terminate any of the children, nor does it remove events. 
 */
void
kita_free(kita_state_s** state)
{
	for (size_t i = 0; i < (*state)->num_children; ++i)
	{
		kita_child_free(&(*state)->children[i]);
	}

	free(*state);
	*state = NULL;
}

kita_state_s* 
kita_init()
{
	// Allocate memory for the state struct
	kita_state_s *s = malloc(sizeof(kita_state_s));
	if (s == NULL) 
	{
		return NULL;
	}
	
	// Set the memory to a zero-initialized struct
	*s = (kita_state_s) { 0 };

	// Initialize an epoll instance
	if (libkita_init_epoll(s) != 0)
	{
		return NULL;
	}

	// Return a pointer to the created state struct
	return s;
}

#endif /* KITA_H */
#endif /* KITA_IMPLEMENTATION */
