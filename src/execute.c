#include <spawn.h>     // posix_spawnp()
#include <wordexp.h>   // wordexp(), wordfree(), ...
#include <sys/types.h> // pid_t

extern char **environ; // Required to pass the environment to child cmds

/*
 * Run the given command via posix_spawnp() in a 'fire and forget' manner.
 * Returns the PID of the spawned process or -1 if running it failed.
 */
pid_t run_cmd(const char *cmd)
{
	// Return early if cmd is NULL or empty
	if (cmd == NULL || cmd[0] == '\0')
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

