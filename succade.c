#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include "ini.h"

#define DEBUG 0
#define NAME "succade"
#define BLOCKS_DIR "blocks"
#define BAR_PROCESS "lemonbar"

struct block
{
	char *name;
	char *path;
	FILE *fd;
	char *fg;
	char *bg;
	size_t offset : 16;
	char *label;
	char *trigger;
	int used : 1;
	double reload;
	double waited;
	char *input;
	char *result;
};

struct trigger
{
	char *cmd;
	FILE *fd;
	struct block *b;
	int ready : 1;
};

struct bar
{
	char *name;
	FILE *fd;
	char *fg;
	char *bg;
	char *prefix;
	char *suffix;
	size_t width : 16;
	size_t height : 16;
	size_t x : 16;
	size_t y : 16;
	int bottom : 1;
	int force : 1;
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
		(b->fg && strlen(b->fg)) ? b->fg : "-", 
		(b->bg && strlen(b->bg)) ? b->bg : "-", 
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
	free(b->fg);
	b->fg = NULL;

	free(b->bg);
	b->bg = NULL;

	free(b->prefix);
	b->prefix = NULL;

	free(b->suffix);
	b->suffix = NULL;
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
	if (b->input)
	{
		size_t cmd_len = strlen(b->path) + strlen(b->input) + 4;
		char *cmd = malloc(cmd_len);
		snprintf(cmd, cmd_len, "%s '%s'", b->path, b->input);
		b->fd = popen(cmd, "r");
		free(cmd);
	}
	else
	{
		b->fd = popen(b->path, "r");
	}
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

int open_trigger(struct trigger *t)
{
	printf("Opening trigger: %s\n", t->cmd);
	t->fd = popen(t->cmd, "r");
	if (t->fd == NULL)
	{
		printf("Failed to open trigger: %s\n", t->cmd);
		return 0;
	}
	setlinebuf(t->fd);
	int fn = fileno(t->fd);
	int flags;
	flags = fcntl(fn, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(fn, F_SETFL, flags);
	return 1;
}

int close_trigger(struct trigger *t)
{
	if (t->fd == NULL)
	{
		return 0;
	}
	pclose(t->fd);
	t->fd = NULL;
	return 1;
}

void open_triggers(struct trigger *triggers, int num_triggers)
{
	for (int i=0; i<num_triggers; ++i)
	{
		open_trigger(&triggers[i]);
	}
}

void close_triggers(struct trigger *triggers, int num_triggers)
{
	for (int i=0; i<num_triggers; ++i)
	{
		close_trigger(&triggers[i]);
	}
}

int free_block(struct block *b)
{
	free(b->name);
	b->name = NULL;

	free(b->path);
	b->path = NULL;

	free(b->fg);
	b->fg = NULL;

	free(b->bg);
	b->bg = NULL;

	free(b->label);
	b->label = NULL;

	free(b->trigger);
	b->trigger = NULL;

	free(b->input);
	b->input = NULL;

	free(b->result);
	b->result = NULL;
}

int free_trigger(struct trigger *t)
{
		free(t->cmd);
		t->cmd = NULL;

		t->b = NULL;
}

void free_blocks(struct block *blocks, int num_blocks)
{
	for (int i=0; i<num_blocks; ++i)
	{
		free_block(&blocks[i]);
	}
}

void free_triggers(struct trigger *triggers, int num_triggers)
{
	for (int i=0; i<num_triggers; ++i)
	{
		free_trigger(&triggers[i]);
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
	b->used = 1; // Mark this block as having run at least once
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
		if (!blocks[i].used || blocks[i].input || blocks[i].waited >= blocks[i].reload)
		{
			num_blocks_executed += run_block(&blocks[i], 64);
		}
		if (blocks[i].input == NULL && (blocks[i].reload - blocks[i].waited) < until_next)
		{
			until_next = blocks[i].reload - blocks[i].waited;
		}

		char *block_str = malloc(128);
		snprintf(block_str, 128, "%%{O%d}%%{F%s}%%{B%s}%s%s%s%s%{F-}%{B-}",
			blocks[i].offset,
			blocks[i].fg && strlen(blocks[i].fg) ? blocks[i].fg : "-",
			blocks[i].bg && strlen(blocks[i].bg) ? blocks[i].bg : "-",
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
		if (DEBUG)
		{
			printf("%s\n", lemonbar_str);
		}
		strcat(lemonbar_str, "\n");
		fputs(lemonbar_str, b->fd);
		return 1;
	}
	return 0;
}

// untested & unused
char *blockstr(const struct bar *bar, const struct block *b, size_t len)
{
	char *block_str = malloc(len);
	snprintf(block_str, len, "%%{O%d}%%{F%s}%%{B%s}%s%s%s%s%{F-}%{B-}",
		b->offset,
		b->fg && strlen(b->fg) ? b->fg : "-",
		b->bg && strlen(b->bg) ? b->bg : "-",
		bar->prefix ? bar->prefix : "",
		b->label ? b->label : "",
		b->result,
		bar->suffix ? bar->suffix : ""
	);
	return block_str;
}

// untested & unused
char *barstr(const struct bar *bar, const struct block *blocks, size_t num_blocks)
{
	size_t blockstr_len = 256;
	char *bar_str = malloc(blockstr_len * num_blocks);
	bar_str[0] = '\0';

	for (int i=0; i<num_blocks; ++i)
	{
		char *block_str = blockstr(bar, &blocks[i], blockstr_len);
		strcat(bar_str, block_str);
		free(block_str);
	}
	strcat(bar_str, "\n");
	bar_str = realloc(bar_str, strlen(bar_str) + 1);
	return bar_str;
}

static int bar_ini_handler(void *b, const char *section, const char *name, const char *value)
{
	struct bar *bar = (struct bar*) b;
	if (equals(name, "name"))
	{
		bar->name = strdup(value);
		return 1;
	}
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		bar->fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		bar->bg = is_quoted(value) ? unquote(value) : strdup(value);
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
	if (equals(name, "x"))
	{
		bar->x = atoi(value);
		return 1;
	}
	if (equals(name, "y"))
	{
		bar->y = atoi(value);
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
	if (equals(name, "prefix") || equals(name, "block-prefix"))
	{
		bar->prefix = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "suffix") || equals(name, "block-suffix"))
	{
		bar->suffix = is_quoted(value) ? unquote(value) : strdup(value);
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
		block->fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		block->bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "offset"))
	{
		block->offset = atoi(value);
	}
	if (equals(name, "label"))
	{
		block->label = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "reload"))
	{
		if (is_quoted(value)) // String means trigger!
		{
			block->trigger = unquote(value);
		}
		else
		{
			block->reload = atof(value);
		}
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

int is_executable(const char *filename)
{
	return 1;
	// TODO this needs the PATH of the file as well...
	struct stat sb;
	return (stat(filename, &sb) == 0 && sb.st_mode & S_IXUSR);
}

int probably_a_block(const char *filename)
{
	return !is_ini(filename) && !is_hidden(filename) && is_executable(filename);
}

int create_triggers(struct trigger **triggers, struct block *blocks, int num_blocks)
{
	if (num_blocks == 0)
	{
		*triggers = NULL;
		return 0;
	}
	*triggers = malloc(num_blocks * sizeof(struct trigger));
	int num_triggers_created = 0;
	for (int i=0; i<num_blocks; ++i)
	{
		if (blocks[i].trigger == NULL)
		{
			continue;
		}
		struct trigger t = {
			.cmd = strdup(blocks[i].trigger),
			.fd = NULL,
			.b = &blocks[i],
			.ready = 0
		};
		*triggers[num_triggers_created++] = t;
	}
	*triggers = realloc(*triggers, num_triggers_created * sizeof(struct trigger));
	return num_triggers_created;
}

int create_blocks(struct block **blocks, const char *blockdir)
{
	DIR *block_dir = opendir(blockdir);
	if (block_dir == NULL)
	{
		*blocks = NULL;
		return 0;
	}

	int num_blocks = 0;
	struct dirent *entry;
	while ((entry = readdir(block_dir)) != NULL)
	{
		if (entry->d_type == DT_REG && probably_a_block(entry->d_name))
		{
			++(num_blocks);
		}
	}

	if (num_blocks == 0)
	{
		*blocks = NULL;
		return 0;
	}

	rewinddir(block_dir);
	int num_blocks_created = 0;
	*blocks = malloc(num_blocks * sizeof(struct block));
	while (num_blocks_created < num_blocks && (entry = readdir(block_dir)) != NULL)
	{
		if (entry->d_type == DT_REG && probably_a_block(entry->d_name))
		{
			struct block b = {
				.name = strdup(entry->d_name),
				.path = NULL,
				.fd = NULL,
				.fg = NULL,
				.bg = NULL,
				.offset = 0,
				.label = NULL,
				.used = 0,
				.trigger = NULL,
				.reload = 5.0,
				.waited = 0.0,
				.input = NULL,
				.result = NULL
			};
			size_t path_len = strlen(blockdir) + strlen(b.name) + 2;
			b.path = malloc(path_len);
			snprintf(b.path, path_len, "%s/%s", blockdir, b.name);
			(*blocks)[num_blocks_created++] = b;
		}
	}
	closedir(block_dir);
	return num_blocks_created;
}

int configure_blocks(struct block *blocks, int num_blocks, const char *blocks_dir)
{
	for (int i=0; i<num_blocks; ++i)
	{
		configure_block(&blocks[i], blocks_dir);
	}
}

// NEW and UNTESTED
char *config_dir()
{
	char *home = getenv("HOME");
	char *config_home = getenv("XDF_CONFIG_HOME");
	char *config_dir = NULL;
	if (config_home == NULL)
	{
		size_t config_dir_len = strlen(home) + strlen(".config") + strlen(NAME) + 3;
		config_dir = malloc(config_dir_len);
		snprintf(config_dir, config_dir_len, "%s/%s/%s", home, ".config", NAME);
	}
	else
	{
		size_t config_dir_len = strlen(config_home) + strlen(NAME) + 2;
		config_dir = malloc(config_dir_len);
		snprintf(config_dir, config_dir_len, "%s/%s", config_home, NAME);
	}
	return config_dir;
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

int run_trigger(struct trigger *t)
{
	if (t->fd == NULL)
	{
		return 0;
	}
	char res[256];
	
	int num_lines = 0;
	while (fgets(res, 256, t->fd) != NULL)
	{
		++num_lines;
	}
	if (num_lines)
	{
		struct block *b = t->b;
		if (b->input != NULL)
		{
			free(b->input);
			b->input = NULL;
		}
		b->input = strdup(res);
	}
		
	/*
	if (fgets(res, 256, t->fd))
	{
		struct block *b = t->b;
		if (b->input != NULL)
		{
			free(b->input);
			b->input = NULL;
		}
		b->input = strdup(res);
		return 1;
	}
	return 0;
	*/
	return num_lines;
}

int main(void)
{
	if (DEBUG)
	{
		printf("sizeof bar: %d\n", sizeof(struct bar));
		printf("sizeof block: %d\n", sizeof(struct block));
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

	struct block *blocks;
	int num_blocks = create_blocks(&blocks, blocksdir);
	configure_blocks(blocks, num_blocks, blocksdir);

	struct trigger *triggers;
	int num_triggers = create_triggers(&triggers, blocks, num_blocks);

	printf("Blocks found: ");
	for (int i=0; i<num_blocks; ++i)
	{
		printf("%s ", blocks[i].name);
	}
	printf("\n");

	printf("Number of triggers: %d\n", num_triggers);
	open_triggers(triggers, num_triggers);

	/* MAIN LOGIC/LOOP */

	struct bar lemonbar = {
		.name = NULL,
		.fd = NULL,
		.fg = NULL,
		.bg = NULL,
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
		printf("Failed to load RC file: %src\n", NAME);
		exit(1);
	}
	open_bar(&lemonbar);
	if (lemonbar.fd == NULL)
	{
		printf("Failed to open bar: %s\n", BAR_PROCESS);
		exit(1);
	}

	int epfd = epoll_create(1);
	if (epfd < 0)
	{
		printf("Could not create epoll file descriptor\n");
		exit(1);
	}

	for (int i=0; i<num_triggers; ++i)
	{
		struct epoll_event eev = { 0 };
		eev.data.ptr = &triggers[i];
		eev.events = EPOLLIN | EPOLLET;
		epoll_ctl(epfd, EPOLL_CTL_ADD, fileno(triggers[i].fd), &eev);
		//int epctl = epoll_ctl(epfd, EPOLL_CTL_ADD, fileno(triggers[i].fd), &eev);
	}

	double now;
	double before = get_time();
	double delta;
	double wait;

	while (1)
	{
		now = get_time();
		delta = now - before;
		before = now;
	//	printf("Seconds elapsed: %f\n", delta);
		
		struct epoll_event tev[num_triggers];
		int num_events = epoll_wait(epfd, tev, num_triggers, wait * 1000);
		if (DEBUG) printf("num events: %d\n", num_events);
	
		for (int i=0; i<num_events;++i)
		{
			if (tev[i].events & EPOLLIN)
			{
				((struct trigger*) tev[i].data.ptr)->ready = 1;
				//run_trigger((struct trigger*) tev[i].data.ptr);
			}
		}	

		for (int i=0; i<num_triggers; ++i)
		{
			if (triggers[i].ready)
			{
				run_trigger(&triggers[i]);
			}
		}

		feed_bar(&lemonbar, blocks, num_blocks, delta, &wait);
	//	printf("Next in %f seconds\n", wait);
		//usleep(wait * 1000000.0); // microseconds (1 us = 1000 ms)
	}
	close(epfd);
	free_blocks(blocks, num_blocks);
	free(blocks);
	close_triggers(triggers, num_triggers);
	free_triggers(triggers, num_triggers);
	free(triggers);
	close_bar(&lemonbar);
	free_bar(&lemonbar);
	return 0;
}
