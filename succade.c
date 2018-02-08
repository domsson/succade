#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include "succade.h"
#include "ini.h"

#define NAME "succade"
#define BLOCKS_DIR "blocks"
#define DEFAULT_FG "#333333"
#define DEFAULT_BG "#EEEEEE"
#define BAR_PROCESS "lemonbar"

struct block
{
	char *name;
	char *path;
	FILE *fd;
	char fg[16];
	char bg[16];
	char align;
	char *label;
	int ran;
	double reload;
	double waited;
	char *result;
};

struct trigger
{
	char *cmd;
	FILE *fd;
	struct block *b;
};

struct bar
{
	char *name;
	FILE *fd;
	char fg[16];
	char bg[16];
	size_t width;
	size_t height;
	size_t x;
	size_t y;
	int bottom;
	int force;
	char *prefix;
	char *suffix;
};

/*
 * Returns 1 if both input strings are equal, otherwise 0.
 */
int equals(const char *str1, const char *str2)
{
	return strcmp(str1, str2) == 0;
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
	if (first == '"' && last == '"') return 1;   // Double-quoted string
	return 0;
}

/*
 * Returns a pointer to a string that is the same as the input string, 
 * minus the enclosing quotation chars (either single or double quotes).
 * The pointer is allocated with malloc(), the caller needs to free it!
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

int open_bar(struct bar *b)
{
	char width[8];
	char height[8];

	snprintf(width, 8, "%d", b->width);
	snprintf(height, 8, "%d", b->height);

	char barprocess[512];
	snprintf(barprocess, 512, "%s -g %sx%s+%d+%d -F%s -B%s %s %s",
		BAR_PROCESS,
		(b->width > 0) ? width : "",
		(b->height > 0) ? height : "",
		b->x,
		b->y,
		(b->fg && strlen(b->fg)) ? b->fg : DEFAULT_FG, 
		(b->bg && strlen(b->bg)) ? b->bg : DEFAULT_BG,
		(b->bottom) ? "-b" : "",
		(b->force)  ? "-f" : ""
	);

	printf("Bar process: %s\n", barprocess);	

	// Run lemonbar via popen() in write mode,
	// this enables us to send data to lemonbar's stdin
	b->fd = popen(barprocess, "w");

	if (b->fd == NULL)
	{
		return 0;
	}	

	// The stream is usually unbuffered, so we would have
	// to call fflush(stream) after each and every line,
	// instead we set the stream to be line buffered.
	setlinebuf(b->fd);
	return 1;
}

void free_bar(struct bar *b)
{
	if (b->prefix != NULL)
	{
		free(b->prefix);
	}
	if (b->suffix != NULL)
	{
		free(b->suffix);
	}
}

void close_bar(struct bar *b)
{
	if (b->fd != NULL)
	{
		pclose(b->fd);
	}
}

int open_block(struct block *b)
{
	b->fd = popen(b->path, "r");
	return (b->fd == NULL) ? 0 : 1;
}

int close_block(struct block *b)
{
	if (b->fd == NULL)
	{
		return 0;
	}
	pclose(b->fd);
	b->fd = NULL;	
	return 1;
}

void open_blocks(struct block *blocks, int num_blocks)
{
	for(int i=0; i<num_blocks; ++i)
	{
		open_block(&blocks[i]);
	}
}

void close_blocks(struct block *blocks, int num_blocks)
{
	for(int i=0; i<num_blocks; ++i)
	{
		close_block(&blocks[i]);
	}
}

int free_block(struct block *b)
{
	if (b->name != NULL)
	{
		free(b->name);
		b->name = NULL;
	}
	if (b->path != NULL)
	{
		free(b->path);
		b->path = NULL;
	}
	if (b->label != NULL)
	{
		free(b->label);
		b->label = NULL;
	}
	if (b->result != NULL)
	{
		free(b->result);
		b->result = NULL;
	}
	return 1;
}

void free_blocks(struct block *blocks, int num_blocks)
{
	for (int i=0; i<num_blocks; ++i)
	{
		free_block(&blocks[i]);
	}
}

int run_block(struct block *b, size_t result_length)
{
	open_block(b);
	if (b->fd == NULL)
	{
		printf("Block is dead: `%s`", b->name);
		return 0;
	}
	if (b->result != NULL)
	{
		free(b->result);
		b->result = NULL;
	}
	b->result = malloc(result_length);
	if (fgets(b->result, result_length, b->fd) == NULL)
	{
		printf("Unable to fetch input from block: `%s`", b->name);
		close_block(b);
		return 0;
	}
	b->result[strcspn(b->result, "\n")] = 0; // Remove '\n'
	b->ran = 1; // Mark this block as having run at least once
	b->waited = 0.0; // This block was last run... now!
	close_block(b);
	return 1;
}

int feed_bar(struct bar *b, struct block *blocks, int num_blocks, double delta, double *next)
{
	if (b->fd == NULL)
	{
		perror("Bar seems dead");
		return 0;
	}

	char lemonbar_str[1024];
	lemonbar_str[0] = '\0';

        int num_blocks_executed = 0;	
	double until_next = 5;

	for(int i=0; i<num_blocks; ++i)
	{
		blocks[i].waited += delta;
		if (!blocks[i].ran || blocks[i].waited >= blocks[i].reload)
		{
			num_blocks_executed += run_block(&blocks[i], 64);
		}
		if ((blocks[i].reload - blocks[i].waited) < until_next)
		{
			until_next = blocks[i].reload - blocks[i].waited;
		}

		char *block_str = malloc(128);
		snprintf(block_str, 128, "%%{F%s}%%{B%s}%s%s%s%s%{F-}%{B-}",
			strlen(blocks[i].fg) ? blocks[i].fg : "-",
			strlen(blocks[i].bg) ? blocks[i].bg : "-",
			b->prefix ? b->prefix : "",
			blocks[i].label ? blocks[i].label : "",
			blocks[i].result,
			b->suffix ? b->suffix : ""
		);
		
		strcat(lemonbar_str, block_str);
		free(block_str);
	}
	*next = until_next;

	if (num_blocks_executed)
	{
		printf("%s\n", lemonbar_str);
		strcat(lemonbar_str, "\n");
		fputs(lemonbar_str, b->fd);
		return 1;
	}
	return 0;
}

static int bar_ini_handler(void *b, const char *section, const char *name, const char *value)
{
	struct bar *bar = (struct bar*) b;
	if (equals(name, "name"))
	{
		bar->name = strdup(value);
	}
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		strcpy(bar->fg, value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		strcpy(bar->bg, value);
		return 1;
	}
	if (equals(name, "h") || equals(name, "height"))
	{
		bar->height = atoi(value);
		return 1;
	}
	if (equals(name, "w") || equals(name, "width"))
	{
		bar->width = atoi(value);
		return 1;
	}
	if (equals(name, "dock"))
	{
		bar->bottom = (equals(value, "bottom")) ? 1 : 0;
		return 1;
	}
	if (equals(name, "force"))
	{
		bar->force = (equals(value, "true")) ? 1 : 0;
		return 1;
	}
	if (equals(name, "prefix"))
	{
		if (is_quoted(value))
	       	{
			bar->prefix = unquote(value);
	       	}
		else
		{
			bar->prefix = strdup(value);
		}
		return 1;
	}
	if (equals(name, "suffix"))
	{
		if (is_quoted(value))
		{
			bar->suffix = unquote(value);
		}
		else
		{
			bar->suffix = strdup(value);
		}
		return 1;
	}
	return 0; // unknown section/name, error
}

int configure_bar(struct bar *b, const char *config_dir)
{
	char rc[256];
	snprintf(rc, sizeof(rc), "%s/%src", config_dir, NAME);
	if (ini_parse(rc, bar_ini_handler, b) < 0)
	{
		printf("Can't parse rc file %s\n", rc);
		return 0;
	}
	return 1;
}

static int block_ini_handler(void *b, const char *section, const char *name, const char *value)
{
	struct block *block = (struct block*) b;
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		strcpy(block->fg, value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		strcpy(block->bg, value);
		return 1;
	}
	if (equals(name, "label"))
	{
		if (is_quoted(value))
		{
			block->label = unquote(value);
		}
		else	
		{
			block->label = strdup(value);
		}
		return 1;
	}
	if (equals(name, "reload"))
	{
		block->reload = atof(value);
		return 1;
	}
	return 0; // unknown section/name or error
}

int configure_block(struct block *b, const char *blocks_dir)
{
	char blockini[256];
	snprintf(blockini, sizeof(blockini), "%s/%s.%s", blocks_dir, b->name, "ini");
	if (ini_parse(blockini, block_ini_handler, b) < 0)
	{
		printf("Can't parse block INI: %s\n", blockini);
		return 0;
	}
	return 1;
}

int is_ini(const char *filename)
{
	char *dot = strrchr(filename, '.');
	return (dot && !strcmp(dot, ".ini")) ? 1 : 0;
}

int is_hidden(const char *filename)
{
	return filename[0] == '.';
}

int count_blocks(const char *blockdir)
{
	DIR *block_dir = opendir(blockdir);
	int count = 0;
	struct dirent *entry;
	while ((entry = readdir(block_dir)) != NULL)
	{
		if (entry->d_type == DT_REG && !is_ini(entry->d_name) && !is_hidden(entry->d_name))
		{
			++count;
		}
	}
	closedir(block_dir);
	return count;
}

int init_blocks(const char *blockdir, struct block *blocks, int num_blocks)
{
	DIR *block_dir = opendir(blockdir);
	struct dirent *entry;
	int i = 0;
	while ((entry = readdir(block_dir)) != NULL)
	{
		if (entry->d_type == DT_REG && !is_ini(entry->d_name) && !is_hidden(entry->d_name))
		{
			if (i < num_blocks)
			{
				struct block b = {
					.name = strdup(entry->d_name),
					.path = NULL,
					.fd = NULL,
					.fg = { 0 },
					.bg = { 0 },
					.align = 0,
					.label = NULL,
					.ran = 0,
					.reload = 0.0,
					.waited = 0.0,
					.result = NULL
				};
				size_t path_len = strlen(blockdir) + strlen(b.name) + 2;
				b.path = malloc(path_len);
				snprintf(b.path, path_len, "%s/%s", blockdir, b.name);
				blocks[i++] = b;
			}
			else
			{
				perror("Can't create block, not enough space");
			}
		}
	}
	closedir(block_dir);
	return i;
}

int configure_blocks(struct block *blocks, int num_blocks, const char *blocks_dir)
{
	int i;
	for (i=0; i<num_blocks; ++i)
	{
		configure_block(&blocks[i], blocks_dir);
	}
}

int get_config_dir(char *buffer, int buffer_size)
{
	char *config_home = getenv("XDF_CONFIG_HOME");
	if (config_home != NULL)
	{
		return snprintf(buffer, buffer_size, "%s/%s", config_home, NAME);
	}
	else
	{
		return snprintf(buffer, buffer_size, "%s/%s/%s", getenv("HOME"), ".config", NAME);
	}
}

int get_blocks_dir(char *buffer, int buffer_size)
{
	char *config_home = getenv("XDG_CONFIG_HOME");
	if (config_home != NULL)
	{
		return snprintf(buffer, buffer_size, "%s/%s/%s", config_home, NAME, BLOCKS_DIR);
	}
	else
	{
		return snprintf(buffer, buffer_size, "%s/%s/%s/%s", getenv("HOME"), ".config", NAME, BLOCKS_DIR);
	}
}

int smallest_reload(struct block *blocks, size_t num_blocks)
{
	int smallest = 5;
	for (int i=0; i<num_blocks; ++i)
	{
		if (blocks[i].reload < smallest)
		{
			smallest = blocks[i].reload;
		}
	}
	return smallest;
}

double get_time()
{
	clockid_t cid = (sysconf(_SC_MONOTONIC_CLOCK) > 0) ? CLOCK_MONOTONIC : CLOCK_REALTIME;
	struct timespec ts;
	clock_gettime(cid, &ts);
	return (double) ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

int main(void)
{
	char configdir[256];
	if (get_config_dir(configdir, sizeof(configdir)))
	{
		printf("Config: %s\n", configdir);
	}

	char blocksdir[256];
	if (get_blocks_dir(blocksdir, sizeof(blocksdir)))
	{
		printf("Blocks: %s\n", blocksdir);
	}

	DIR *dir;
	dir = opendir(blocksdir);
	if (dir == NULL)
	{
		perror("Could not open config dir");
		return -1;
	}
	closedir(dir);

	int num_blocks = count_blocks(blocksdir);
	printf("%d files in blocks dir\n", num_blocks);

	struct block blocks[num_blocks];
	int num_blocks_found = init_blocks(blocksdir, blocks, num_blocks);
	configure_blocks(blocks, num_blocks_found, blocksdir);

	printf("Blocks found: ");
	for (int i=0; i<num_blocks_found; ++i)
	{
		printf("%s ", blocks[i].name);
	}
	printf("\n");

	/* MAIN LOGIC/LOOP */

	struct bar lemonbar = {
		.name = NULL,
		.fd = NULL,
		.fg = { 0 },
		.bg = { 0 },
		.width = 0,
		.height = 0,
		.x = 0,
		.y = 0,
		.bottom = 0,
		.force = 0,
		.prefix = NULL,
		.suffix = NULL
	};	
	if (!configure_bar(&lemonbar, configdir))
	{
		perror("Could not load RC file");
		exit(1);
	}
	open_bar(&lemonbar);
	if (lemonbar.fd == NULL)
	{
		perror("Could not open bar process. Is lemonbar installed?");
		exit(1);
	}

	double now;
	double before = get_time();
	double delta;
	double next;

	while (1)
	{
		now = get_time();
		delta = now - before;
		before = now;
		printf("Seconds elapsed: %f\n", delta);
		
		//open_blocks(blocks, num_blocks_found);
		feed_bar(&lemonbar, blocks, num_blocks_found, delta, &next);
		printf("Next in %f seconds\n", next);
		//close_blocks(blocks, num_blocks_found);
		//sleep(1);
		usleep(next * 1000000.0);
	}
	free_blocks(blocks, num_blocks_found);
	close_bar(&lemonbar);
	free_bar(&lemonbar);
	return 0;
}
