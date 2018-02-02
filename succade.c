#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include "succade.h"
#include "ini.h"

#define NAME "succade"

struct block
{
	char name[64];
	FILE *fd;
	char fg[16];
	char bg[16];
	char align;
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
};

int open_bar(struct bar *b)
{
	// Run lemonbar via popen() in write mode,
	// this enables us to send data to lemonbar's stdin
	char barprocess[512];

	char fg[13];
	snprintf(fg, 13, "-F%s", (strlen(b->fg) ? b->fg : "#333333"));
	
	char bg[13];
	snprintf(bg, 13, "-B%s", (strlen(b->bg) ? b->bg : "#EEEEEE"));
	
	char geom[32];
        char width[8];
	char height[8];

	snprintf(width, 8, "%d", b->width);
	snprintf(height, 8, "%d", b->height);

	strcpy(geom, "-g ");
	if (b->width > 0) strcat(geom, width);
	strcat(geom, "x");
	if (b->height > 0) strcat(geom, height);

	snprintf(barprocess, 512, "lemonbar %s %s %s", geom, fg, bg);

	if (b->bottom)
	{
		strcat(barprocess, " -b");
	}

	if (b->force)
	{
		strcat(barprocess, " -f");
	}

	printf("Bar process: %s\n", barprocess);	

	b->fd = popen(barprocess, "w");
	
	if (b->fd != NULL)
	{
		// The stream is usually unbuffered, so we would have
		// to call fflush(stream) after each and every line,
		// instead we set the stream to be line buffered.
		setlinebuf(b->fd);
		return 1;
	}

	return 0;
}

void close_bar(struct bar *b)
{
	if (b->fd != NULL)
	{
		pclose(b->fd);
	}
}

void open_blocks(struct block *blocks, int num_blocks, const char *blocks_dir)
{
	int i;
	for(i=0; i<num_blocks; ++i)
	{
		// block path length = blocks dir length + '/' + block name length + '\0'
		size_t block_path_length = strlen(blocks_dir) + 1 + strlen(blocks[i].name) + 1;
		char *block_path = malloc(block_path_length);
		snprintf(block_path, block_path_length, "%s/%s", blocks_dir, blocks[i].name);
		blocks[i].fd = popen(block_path, "r");
		free(block_path);
	}
}

void close_blocks(struct block *blocks, int num_blocks)
{
	int i;
	for(i=0; i<num_blocks; ++i)
	{
		pclose(blocks[i].fd);
	}
}

void bar(FILE *stream, struct block *blocks, int num_blocks)
{
	char lemonbar_str[1024];

	lemonbar_str[0] = '\0';
 
	int i;	
	for(i=0; i<num_blocks; ++i)
	{
		char *block_res = malloc(64);
		run_block(blocks[i].fd, block_res, 64);

		char *fg = malloc(16);
		snprintf(fg, 16, "%{F%s}", (strlen(blocks[i].fg) ? blocks[i].fg : "-"));
		strcat(lemonbar_str, fg);
		free(fg);

		char *bg = malloc(16);
		snprintf(bg, 16, "%{B%s}", (strlen(blocks[i].bg) ? blocks[i].bg : "-"));
		strcat(lemonbar_str, bg);
		free(bg);

		block_res[strcspn(block_res, "\n")] = 0; // Remove '\n'
		strcat(lemonbar_str, block_res);
		free(block_res);
	}

	printf("%s\n", lemonbar_str);
	strcat(lemonbar_str, "\n");
	fputs(lemonbar_str, stream);
}

int run_block(FILE *blockfd, char *result, int result_length)
{
	if (blockfd == NULL)
	{
		perror("Block is dead");
		return 0;
	}
	if (fgets(result, result_length, blockfd) == NULL)
	{
		perror("Unable to fetch input from block");
		return 0;
	}
	return 1;
}

int is_ini(char *filename)
{
	char *dot = strrchr(filename, '.');
	return (dot && !strcmp(dot, ".ini")) ? 1 : 0;
}

int equals(const char *str1, const char *str2)
{
	return strcmp(str1, str2) == 0;
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

int count_blocks(DIR *dir)
{
	int count = 0;
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL)
	{
		if (entry->d_type == DT_REG && !is_ini(entry->d_name))
		{
			++count;
		}
	}
	rewinddir(dir);
	return count;
}

int init_blocks(DIR *block_dir, struct block *blocks, int num_blocks)
{
	struct dirent *entry;
	int i = 0;
	while ((entry = readdir(block_dir)) != NULL)
	{
		if(entry->d_type == DT_REG && !is_ini(entry->d_name))
		{
			if (i < num_blocks)
			{
				struct block b;
				strcpy(b.name, entry->d_name);
				blocks[i++] = b;
			}
			else
			{
				perror("Can't create block, not enough space");
			}
		}
	}
	rewinddir(block_dir);
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
		return snprintf(buffer, buffer_size, "%s/%s/%s", config_home, NAME, "blocks");
	}
	else
	{
		return snprintf(buffer, buffer_size, "%s/%s/%s/%s", getenv("HOME"), ".config", NAME, "blocks");
	}
}

int main(void)
{
	char *homedir = getenv("HOME");
	if (homedir != NULL)
	{
		printf("Home: %s\n", homedir);
	}

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

	int num_blocks = count_blocks(dir);
	printf("%d files in blocks dir\n", num_blocks);

	struct block blocks[num_blocks];
	int num_blocks_found = init_blocks(dir, blocks, num_blocks);
	configure_blocks(blocks, num_blocks_found, blocksdir);

	closedir(dir);

	printf("Blocks found: ");
	int i;
	for (i=0; i<num_blocks_found; ++i)
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
	//FILE *lemonbar = open_bar();
	open_bar(&lemonbar);
	//if (lemonbar == NULL)
	if (lemonbar.fd == NULL)
	{
		perror("Could not open bar process");
		exit(1);
	}
	
	while (1)
	{
		open_blocks(blocks, num_blocks_found, blocksdir);
		//bar(lemonbar, blocks, num_blocks_found);
		bar(lemonbar.fd, blocks, num_blocks_found);
		close_blocks(blocks, num_blocks_found);
		sleep(1);
	}
	//close_bar(lemonbar);
	close_bar(&lemonbar);
	return 0;
}
