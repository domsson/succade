#ifndef SUCCADE_H
#define SUCCADE_H

struct succade_config
{
	char *binary;		// Custom bar binary
	char *config;		// Custom config file
	int pipe : 1;		// Pipe mode (print to stdout instead of feedin bar)
	int help : 1;		// Help mode (print help text, then exit)
};

#endif 
