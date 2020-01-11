#ifndef SUCCADE_H
#define SUCCADE_H

struct succade_config
{
	char *binary;  // Custom bar binary
	char *config;  // Custom config file
	int empty : 1; // Run the bar even if no blocks are present
	int pipe : 1;  // Pipe mode (print to stdout instead of feeding bar)
	int help : 1;  // Help mode (print help text, then exit)
	
	// We need the following for when we hand user data to the inih handler
	struct bar *bar;                 // Reference to the bar
	struct block_container *blocks;  // Reference to the blocks container
};

#endif 
