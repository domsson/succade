#include <unistd.h>
#include "succade.h"

void parse_args(int argc, char **argv, struct succade_config *cfg)
{
	// Get arguments, if any
	opterr = 0;
	int o;
	while ((o = getopt(argc, argv, "b:c:eph")) != -1)
	{
		switch (o)
		{
			case 'b': // bar/binary (custom bar binary)
				cfg->binary = optarg;
				break;
			case 'c': // config (custom config file location)
				cfg->config = optarg;
				break;
			case 'e': // empty (run bar even if no blocks present)
				cfg->empty = 1;
				break;
			case 'p': // pipe/print (dump output to stdout)
				cfg->pipe = 1;
				break;
			case 'h': // help (show help)
				cfg->help = 1;
				break;
		}
	}
}

