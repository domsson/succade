#include <stdio.h>     // fdopen(), FILE, ...
#include <unistd.h>    // pipe(), fork(), dup(), close(), _exit(), ...
#include <string.h>    // strlen()
#include <spawn.h>     // posix_spawnp()
#include <wordexp.h>   // wordexp(), wordfree(), ...
#include <sys/types.h> // pid_t

extern char **environ; // Required to pass the environment to child cmds

/*
 * TODO we're using execvp() and posix_spawnp() instead of execv() and 
 *      posix_spawn(). The difference is that the latter expect an absolute 
 *      or relative file path to the executable, while the former expect to 
 *      simply receive a filename which will then be searched for in PATH.
 *      The question is, which one actually makes more sense for us to use?
 *      Can the former _also_ handle file paths? Or do we need to look at 
 *      the commands given, figure out if they are a path, then call one or 
 *      the other function accordingly? Some more testing is required here!
 */

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
pid_t popen_noshell(const char *cmd, FILE **in, FILE **out, FILE **err)
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
 * Run the given command via posix_spawnp() in a 'fire and forget' manner.
 * Returns the PID of the spawned process or -1 if running it failed.
 */
pid_t run_cmd(const char *cmd)
{
	// Return early if cmd is NULL or empty
	if (cmd == NULL || !strlen(cmd))
	{
		return -1;
	}

	// Try to parse the command (expand symbols like . and ~ etc)
	wordexp_t p;
	if (wordexp(cmd, &p, 0) != 0)
	{
		return -1;
	}
	
	// Spawn a new child process with the given command
	pid_t pid;
	int res = posix_spawnp(&pid, p.we_wordv[0], NULL, NULL, p.we_wordv, environ);
	wordfree(&p);
	
	// Return the child's PID on success, -1 on failure
	return (res == 0 ? pid : -1);
}

