#ifndef SUCCADE_H 
#define SUCCADE_H

struct block;
int run_block(FILE *blockfd, char *result, int result_length);
int count_blocks(DIR *dir);
void configure_block(struct block *b, , const char *blocks_dir);
int init_blocks(DIR *block_dir, struct block *blocks, int num_blocks);
void bar(FILE *stream, struct block *blocks, int num_blocks);
int is_ini(char *filename);
static int block_ini_handler(void *b, const char *section, const char *name, const char *value);

#endif
