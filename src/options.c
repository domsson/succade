#include <unistd.h>    // getopt()
#include "succade.h"   // scd_pref_s

void parse_args(int argc, char **argv, prefs_s *prefs)
{
	// Get arguments, if any
	opterr = 0;
	int o;
	while ((o = getopt(argc, argv, "c:ehs:V")) != -1)
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
			case 's': // section name for bar
				prefs->section = optarg;
				break;
			case 'V': // print version and exit:
				prefs->version = 1;
				break;
		}
	}
}

