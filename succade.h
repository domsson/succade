#ifndef SUCCADE_H 
#define SUCCADE_H

struct block;
int run_blocki(struct block *b, char *result, int num);
int run_block(FILE *blockfd, char *result, int result_length);
int init_blocks(DIR *block_dir, struct block *blocks, int num_blocks);
int count_files(DIR *dir);
void bar(FILE *stream, struct block *blocks, int num_blocks);
void fetch_block_info(struct block *b);
int is_ini(char *filename);
void configure_block(struct block *b);
static int block_ini_handler(void *b, const char *section, const char *name, const char *value);

#endif
