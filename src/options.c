#include <unistd.h>
#include "succade.h"

void parse_args(int argc, char **argv, scd_prefs_s *prefs)
{
	// Get arguments, if any
	opterr = 0;
	int o;
	while ((o = getopt(argc, argv, "c:eh")) != -1)
	{
		switch (o)
		{
			case 'c': // config (custom config file location)
				prefs->config = optarg;
				break;
			case 'e': // empty (run bar even if no blocks present)
				prefs->empty = 1;
				break;
			case 'h': // help (show help)
				prefs->help = 1;
				break;
		}
	}
}

