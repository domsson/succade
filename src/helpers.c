#include <stdio.h>  // snprintf()
#include <stdlib.h> // malloc(), free(), getenv()
#include <string.h> // strlen(), strcmp()
#include <time.h>   // clock_gettime(), clockid_t, struct timespec

/*
 * Returns 1 if both input strings are equal, otherwise 0.
 */
int equals(const char *str1, const char *str2)
{
	return strcmp(str1, str2) == 0;
}

/*
 * Returns 1 if the input string is NULL or empty, otherwise 0.
 */
int empty(const char *str)
{
	return (str == NULL || str[0] == '\0');
}

/*
 * Returns 1 if the input string is quoted, otherwise 0.
 */
int is_quoted(const char *str)
{
	size_t len = strlen(str); // Length without null terminator
	if (len < 2) return 0;    // We need at least two quotes (empty string)
	char first = str[0];
	char last  = str[len - 1];
	if (first == '\'' && last == '\'') return 1; // Single-quoted string
	if (first == '"'  && last == '"')  return 1; // Double-quoted string
	return 0;
}

/*
 * Returns a pointer to a string that is the same as the input string, 
 * minus the enclosing quotation chars (either single or double quotes).
 * The pointer is allocated with malloc(), the caller needs to free it.
 */
char *unquote(const char *str)
{
	char *trimmed = NULL;
	size_t len = strlen(str);
	if (len < 2) // Prevent zero-length allocation
	{
		trimmed = malloc(1); // Make space for null terminator
		trimmed[0] = '\0';   // Add the null terminator
	}
	else
	{
		trimmed = malloc(len-2+1);        // No quotes, null terminator
		strncpy(trimmed, &str[1], len-2); // Copy everything in between
		trimmed[len-2] = '\0';            // Add the null terminator
	}
	return trimmed;
}

/*
 * Escapes the given string `str` by finding all occurences of the character
 * given in `e`, then creating a new string where each occurence of `e` will 
 * have another one prepended in front of it. The new string is allocated with 
 * malloc(), so it is upon the caller to free the result at some point. 
 * If `diff` is not NULL, it will be set to the number of inserted characters, 
 * effectively giving the difference in size between `str` and the result.
 * `str` is assumed to be null terminated, otherwise the behavior is undefined.
 */
char *escape(const char *str, const char e, size_t *diff)
{
	char   c = 0; // current char
	size_t i = 0; // index of current char
	size_t n = 0; // number of `e` chars found

	// Count the occurences of `e` in `str`
	while ((c = str[i]) != '\0')
	{
		if (c == e)
		{
			++n;
		}
		++i;
	}

	// Return the number of `e`s in `str` via `diff`
	if (diff != NULL)
	{
		*diff = n;
	}

	// Allocate memory for the escaped string
	char *escstr = malloc(i + n + 1); 

	// Create the escaped string
	size_t k = 0;
	for (size_t j = 0; j < i; ++j)
	{
		// Insert two `e` if we find one `e` char
		if (str[j] == e)
		{
			escstr[k++] = e;
			escstr[k++] = e;
		}
		// Otherwise just copy the char as is
		else
		{
			escstr[k++] = str[j];
		}
	}
	
	// Add the null terminator and return
	escstr[k] = '\0';
	return escstr;
}

/*
 * Returns the first string, unless it is empty or NULL, in which the same 
 * check is performed on the second string and it will be returned. If the 
 * second string is also NULL or empty, the third string will be returned
 * without any further tests.
 */
const char *strsel(const char *str1, const char *str2,  const char *str3)
{
	if (str1 && strlen(str1))
	{
		return str1;
	}
	if (str2 && strlen(str2))
	{
		return str2;
	}
	return str3;
}

/*
 * Creates a string suitable for use as a command line option, where the given 
 * character `o` will be used for the switch/option and the given string `arg`
 * will be the argument for that option. If the given `arg` string is NULL, an 
 * empty string is returned. The `arg` string will always be placed in quotes. 
 * Make sure to escape all quotes in `arg`, or you will get unexpected results.
 * The returned string is dynamically allocated, please free it at some point.
 */
char *optstr(char o, const char *arg, int space)
{
	char *str = NULL;

	if (arg)
	{
		size_t len = strlen(arg) + 6;
		str = malloc(len);
		snprintf(str, len, "-%c%s\"%s\"", o, (space ? " " : ""), arg);
	}
	else
	{
		str = malloc(1);
		str[0] = '\0';
	}
	return str;
}

/*
 * Concatenates the given directory, file name and file extension strings
 * to a complete path. The fileext argument is optional, it can be set to NULL.
 * Returns a pointer to the concatenated string, allocated via malloc().
 */
char *filepath(const char *dir, const char *filename, const char *fileext)
{
	if (fileext != NULL)
	{
		size_t path_len = strlen(dir) + strlen(filename) + strlen(fileext) + 3;
		char *path = malloc(path_len);
		snprintf(path, path_len, "%s/%s.%s", dir, filename, fileext);
		return path;
	}
	else
	{
		size_t path_len = strlen(dir) + strlen(filename) + 2;
		char *path = malloc(path_len);
		snprintf(path, path_len, "%s/%s", dir, filename);
		return path;
	}
}

/*
 * Returns a pointer to a string that holds the config dir we want to use.
 * This does not check if the dir actually exists. You need to check still.
 * The string is allocated with malloc() and needs to be freed by the caller.
 */
char *config_dir(const char *name)
{
	char *home = getenv("HOME");
	char *cfg_home = getenv("XDF_CONFIG_HOME");
	char *cfg_dir = NULL;
	size_t cfg_dir_len;
	if (cfg_home == NULL)
	{
		cfg_dir_len = strlen(home) + strlen(".config") + strlen(name) + 3;
		cfg_dir = malloc(cfg_dir_len);
		snprintf(cfg_dir, cfg_dir_len, "%s/%s/%s", home, ".config", name);
	}
	else
	{
		cfg_dir_len = strlen(cfg_home) + strlen(name) + 2;
		cfg_dir = malloc(cfg_dir_len);
		snprintf(cfg_dir, cfg_dir_len, "%s/%s", cfg_home, name);
	}
	return cfg_dir;
}

/*
 * Given a file name (base name including extension, if any, but without path),
 * this function concatenates the configuration directory path with the file 
 * name and returns it as a malloc'd string that the caller has to free. 
 * No checks are performed to verify whether the file actually exists and/or 
 * can be read; if required, the caller has to take care of this.
 * The configuration directory is determined via config_dir().
 */
char *config_path(const char *filename, const char *dirname)
{
	char  *cfg_dir      = config_dir(dirname);
	size_t cfg_dir_len  = strlen(cfg_dir);
	size_t cfg_file_len = strlen(filename);
	size_t cfg_path_len = cfg_dir_len + cfg_file_len + 2;
	char  *cfg_path     = malloc(sizeof(char) * cfg_path_len);

	snprintf(cfg_path, cfg_path_len, "%s/%s", cfg_dir, filename);
	free(cfg_dir);

	return cfg_path;
}

double get_time()
{
	clockid_t cid = CLOCK_MONOTONIC;
	// TODO the next line is cool, as CLOCK_MONOTONIC is not
	// present on all systems, where CLOCK_REALTIME is, however
	// I don't want to call sysconf() with every single iteration
	// of the main loop, so let's do this ONCE and remember...
	//clockid_t cid = (sysconf(_SC_MONOTONIC_CLOCK) > 0) ? CLOCK_MONOTONIC : CLOCK_REALTIME;
	struct timespec ts;
	clock_gettime(cid, &ts);
	return (double) ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

