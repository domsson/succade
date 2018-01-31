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

FILE *open_bar()
{
	// Run lemonbar via popen() in write mode,
	// this enables us to send data to lemonbar's stdin
	FILE *stream = popen("lemonbar -g x24 -b -B '#FFFFFF' -F '#000000'", "w");
	
	if (stream != NULL)
	{
		// The stream is usually unbuffered, so we would have
		// to call fflush(stream) after each and every line,
		// instead we set the stream to be line buffered.
		setlinebuf(stream);
	}

	return stream;
}

void close_bar(FILE *bar)
{
	if (bar != NULL)
	{
		pclose(bar);
	}
}

void open_blocks(struct block *blocks, int num_blocks, const *char blocks_dir)
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
		block_res[0] = '\0';
		run_block(blocks[i].fd, block_res, sizeof(block_res));
		char *fg = malloc(16);
		strcpy(fg, "%{F");
		strcat(fg, blocks[i].fg);
		strcat(fg, "}");
		strcat(lemonbar_str, fg);
		char *bg = malloc(16);
		strcpy(bg, "%{B");
		strcat(bg, blocks[i].bg);
		strcat(bg, "}");
		strcat(lemonbar_str, bg);
		/*
		char *a = malloc(4);
		strcpy(a, "%{x");
		strcat(a, "}");
		a[2] = blocks[i].align;
		strcat(lemonbar_str, a);
		*/
		strcat(lemonbar_str, block_res);
		free(block_res);
		free(fg);
		free(bg);
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

void configure_block(struct block *b, const char *blocks_dir)
{
	char blockini[256];
	snprintf(blockini, sizeof(blockini), "%s/%s.%s", blocks_dir, b->name, "ini");
	if(ini_parse(blockini, block_ini_handler, b) < 0)
	{
		printf("Can't parse block INI: %s\n", blockini);
		return;
	}
	printf("Block INI loaded: fg=%s, bg=%s, align=%s\n", b->fg, b->bg, b->align);
	return;		
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
				//configure_block(&b, blocks_dir);

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
		configure_block(&b, blocks_dir);
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

	FILE *lemonbar = open_bar();
	if (lemonbar == NULL)
	{
		perror("Could not open bar process");
		exit(1);
	}
	
	while (1)
	{
		open_blocks(blocks, num_blocks_found, blocksdir);
		bar(lemonbar, blocks, num_blocks_found);
		close_blocks(blocks, num_blocks_found);
		sleep(2);
	}
	close_bar(lemonbar);
	return 0;
}
