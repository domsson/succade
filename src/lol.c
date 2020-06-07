#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <string.h>    // strlen(), strcmp(), ...
#include <float.h>     // DBL_MAX
#include <sys/types.h> // pid_t
#include <sys/wait.h>  // waitpid()
#include <errno.h>     // errno
#include "succade.h"   // defines, structs, all that stuff
#include "helpers.c"   // Helper functions, mostly for strings

static volatile int running;   // Used to stop main loop in case of SIGINT

/*
 * Runs the bar process and opens file descriptors for reading and writing.
 * Returns 0 on success, -1 if bar could not be started.
 */
int open_lemon(lemon_s *lemon)
{
	char *arg = malloc(3 * sizeof(char));

	char *old_arg = kita_child_get_arg(lemon->child);
	free(old_arg);

	kita_child_set_arg(lemon->child, arg);
	return kita_child_open(lemon->child);
}

/*
 * Send a kill signal to the lemon's child process.
 */
void close_lemon(lemon_s *lemon)
{
	kita_child_term(lemon->child);
}

kita_child_s* make_child(kita_state_s *kita, const char *cmd, int in, int out, int err)
{
	// Create child process
	kita_child_s *child = kita_child_new(cmd, in, out, err);
	if (child == NULL)
	{
		return NULL;
	}

	// Add the child to the kita 
	if (kita_child_add(kita, child) == -1)
	{
		kita_child_free(&child);
		return NULL;
	}

	return child;
}

void on_child_error(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_error(): %s\n", ke->child->cmd);
}

void on_child_feedok(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_feedok(): %s\n", ke->child->cmd);
}

void on_child_readok(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_readok(): %s\n", ke->child->cmd);
}

void on_child_exited(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_exited(): %s\n", ke->child->cmd);
}

void on_child_closed(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_closed(): %s\n", ke->child->cmd);
}

void on_child_reaped(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_reaped(): %s\n", ke->child->cmd);
}

void on_child_remove(kita_state_s *ks, kita_event_s *ke)
{
	fprintf(stderr, "on_child_remove(): %s\n", ke->child->cmd);
}

int main(int argc, char **argv)
{
	kita_state_s *kita = kita_init();
	if (kita == NULL)
	{
		fprintf(stderr, "Failed to initialize kita state, aborting.\n");
		return EXIT_FAILURE;
	}
	kita_set_option(kita, KITA_OPT_NO_NEWLINE, 1);

	// 
	// REGISTER CALLBACKS 
	//

	kita_set_callback(kita, KITA_EVT_CHILD_CLOSED, on_child_closed);
	kita_set_callback(kita, KITA_EVT_CHILD_REAPED, on_child_reaped);
	kita_set_callback(kita, KITA_EVT_CHILD_HANGUP, on_child_exited);
	kita_set_callback(kita, KITA_EVT_CHILD_EXITED, on_child_exited);
	kita_set_callback(kita, KITA_EVT_CHILD_REMOVE, on_child_remove);
	kita_set_callback(kita, KITA_EVT_CHILD_FEEDOK, on_child_feedok);
	kita_set_callback(kita, KITA_EVT_CHILD_READOK, on_child_readok);
	kita_set_callback(kita, KITA_EVT_CHILD_ERROR,  on_child_error);

	//
	// BAR
	//

	lemon_s lemonbar = { 0 };
	lemon_s *lemon = &lemonbar;

	// Create the child process and add it to the kita state
	lemon->child = make_child(kita, "lemonbar", 1, 1, 1);
	if (lemon->child == NULL)
	{
		fprintf(stderr, "Failed to create lemon process\n");
		return EXIT_FAILURE;
	}

	// Open (run) the lemon
	if (open_lemon(lemon) == -1)
	{
		fprintf(stderr, "Failed to open lemonbar\n");
		return EXIT_FAILURE;
	}

	kita_child_set_buf_type(lemon->child, KITA_IOS_IN, KITA_BUF_LINE);

	//
	// MAIN LOOP
	//

	double now;
	double before = get_time();
	double delta;
	double wait = 0.0; 
	
	char test[] = "%{A1:datetime_lmb:}%{O0}%{F-}%{B-}%{U-}%{-o+u}%{T3}%{F-}%{B-} %{T2}%{F#333333}%{B-}%{T1}%{F-}%{B-}2020-06-07 19:17:42%{T3}%{F-}%{B-} %{T-}%{F-}%{B-}%{U-}%{-o-u}%{A}\n";

	running = 1;
	
	while (running)
	{
		now    = get_time();
		delta  = now - before;
		before = now;

		fprintf(stderr, "> now = %f, wait = %f, delta = %f\n", now, wait, delta);

		// feed the bar!
		fprintf(stderr, "%s\n", test);
		if (kita_child_feed(lemon->child, test) != 0)
		{
			fprintf(stderr, "error feeding bar :-(\n");
		}

		// let kita check for child events
		int wait_s = wait == -1 ? wait : wait * MILLISEC_PER_SEC;
		fprintf(stderr, "> kita_tick() with wait = %d\n", wait_s);
		kita_tick(kita, wait_s);

		// Update `wait` accordingly (-1 = not waiting on any blocks)
		wait = 1;
	}

	return EXIT_SUCCESS;
}

