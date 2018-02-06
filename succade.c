#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
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
	int reload;
};

struct trigger
{
	char *cmd;
	FILE *fd;
	struct block *b;
};

struct bar
{
	char name[64];
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

void open_blocks(struct block *blocks, int num_blocks, const char *blocks_dir)
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
	return 1;
}

void free_blocks(struct block *blocks, int num_blocks)
{
	for (int i=0; i<num_blocks; ++i)
	{
		free_block(&blocks[i]);
	}
}

int feed_bar(struct bar *b, struct block *blocks, int num_blocks)
{
	if (b->fd == NULL)
	{
		perror("Bar seems dead");
		return 0;
	}

	char lemonbar_str[1024];
	lemonbar_str[0] = '\0';
 
	for(int i=0; i<num_blocks; ++i)
	{
		char *block_res = malloc(64);
		run_block(&blocks[i], block_res, 64);
		block_res[strcspn(block_res, "\n")] = 0; // Remove '\n'

		char *block_str = malloc(128);
		snprintf(block_str, 128, "%%{F%s}%%{B%s}%s%s%s",
			strlen(blocks[i].fg) ? blocks[i].fg : "-",
			strlen(blocks[i].bg) ? blocks[i].bg : "-",
			b->prefix ? b->prefix : "",
			block_res,
			b->suffix ? b->suffix : ""
		);
		
		strcat(lemonbar_str, block_str);
		free(block_res);
		free(block_str);
	}

	printf("%s\n", lemonbar_str);
	strcat(lemonbar_str, "\n");
	fputs(lemonbar_str, b->fd);
	return 1;
}

int run_block(const struct block *b, char *result, int result_length)
{
	if (b->fd == NULL)
	{
		perror("Block is dead");
		return 0;
	}
	if (fgets(result, result_length, b->fd) == NULL)
	{
		perror("Unable to fetch input from block");
		return 0;
	}
	return 1;
}

static int bar_ini_handler(void *b, const char *section, const char *name, const char *value)
{
	struct bar *bar = (struct bar*) b;
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
			char *trimmed = unquote(value);
			bar->prefix = trimmed;
	       	}
		//printf("Bar prefix: %s\n", value);
	}
	if (equals(name, "suffix"))
	{
		if (is_quoted(value))
		{
			char *trimmed = unquote(value);
			bar->suffix = trimmed;
		}
		//printf("Bar suffix: %s\n", value);
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

	printf("RC file loaded: fg=%s, bg=%s, height=%i\n", b->fg, b->bg, b->height);
	return 1;
}

static int block_ini_handler(void *b, const char *section, const char *name, const char *value)
{
	struct block *block = (struct block*) b;
	if (strcmp(name, "fg") == 0)
	{
		strcpy(block->fg, value);
	}
	else if (strcmp(name, "bg") == 0)
	{
		strcpy(block->bg, value);
	}
	return 1;
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

	printf("Block INI loaded: fg=%s, bg=%s, align=%s\n", b->fg, b->bg, b->align);
	return 1;
}

int is_ini(const char *filename)
{
	char *dot = strrchr(filename, '.');
	return (dot && !strcmp(dot, ".ini")) ? 1 : 0;
}

int count_blocks(const char *blockdir)
{
	DIR *block_dir = opendir(blockdir);
	int count = 0;
	struct dirent *entry;
	while ((entry = readdir(block_dir)) != NULL)
	{
		if (entry->d_type == DT_REG && !is_ini(entry->d_name))
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
		if (entry->d_type == DT_REG && !is_ini(entry->d_name))
		{
			if (i < num_blocks)
			{
				struct block b;
				b.name = strdup(entry->d_name);
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

	//int num_blocks = count_blocks(dir);
	int num_blocks = count_blocks(blocksdir);
	printf("%d files in blocks dir\n", num_blocks);

	struct block blocks[num_blocks];
	//int num_blocks_found = init_blocks(dir, blocks, num_blocks);
	int num_blocks_found = init_blocks(blocksdir, blocks, num_blocks);
	configure_blocks(blocks, num_blocks_found, blocksdir);

	//closedir(dir);

	printf("Blocks found: ");
	for (int i=0; i<num_blocks_found; ++i)
	{
		printf("%s ", blocks[i].name);
	}
	printf("\n");

	/* MAIN LOGIC/LOOP */

	struct bar lemonbar;
	lemonbar.width = 0;
	lemonbar.height = 0;
	lemonbar.x = 0;
	lemonbar.y = 0;
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
	
	while (1)
	{
		open_blocks(blocks, num_blocks_found, blocksdir);
		feed_bar(&lemonbar, blocks, num_blocks_found);
		close_blocks(blocks, num_blocks_found);
		sleep(1);
	}
	free_blocks(blocks, num_blocks_found);
	close_bar(&lemonbar);
	free_bar(&lemonbar);
	return 0;
}
