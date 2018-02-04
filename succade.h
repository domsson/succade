#ifndef SUCCADE_H 
#define SUCCADE_H

struct bar;
struct block;
int run_block(const struct block *b, char *result, int result_length);
//int count_blocks(DIR *dir);
int configure_block(struct block *b, const char *blocks_dir);
//int init_blocks(DIR *block_dir, struct block *blocks, int num_blocks);
//int feed_bar(struct bar *b, struct block *blocks, int num_blocks);
int is_ini(const char *filename);
static int block_ini_handler(void *b, const char *section, const char *name, const char *value);

#endif
