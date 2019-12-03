#include <unistd.h>
#include "succade.h"

void parse_args(int argc, char **argv, struct succade_config *cfg)
{
	// Get arguments, if any
	opterr = 0;
	int o;
	while ((o = getopt(argc, argv, "b:c:ph")) != -1)
	{
		switch (o)
		{
			case 'b': // bar/binary (custom bar binary)
				cfg->binary = optarg;
				break;
			case 'c': // config (custom config file location)
				cfg->config = optarg;
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

