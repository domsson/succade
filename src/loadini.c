#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <string.h>    // strlen(), strcmp(), ...
#include <float.h>     // DBL_MAX
#include "ini.h"       // https://github.com/benhoyt/inih
#include "succade.h"   // defines, structs, all that stuff

int lemon_ini_handler(void *data, const char *section, const char *name, const char *value)
{
	// Unpack the data, which should be a lemon_s (struct succade_lemon)
	thing_s *lemon = (thing_s*) data;
	cfg_s *lc = &lemon->cfg;

	// Check for `name` and set the appropriate property
	if (equals(name, "name"))
	{
		cfg_set_str(lc, LEMON_OPT_NAME, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "bin") || equals(name, "cmd") || equals(name, "command"))
	{
		cfg_set_str(lc, LEMON_OPT_BIN, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		cfg_set_str(lc, LEMON_OPT_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		cfg_set_str(lc, LEMON_OPT_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "lc") || equals(name, "line") || equals(name, "line-color"))
	{
		cfg_set_str(lc, LEMON_OPT_LC, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "lw") || equals(name, "line-width"))
	{
		cfg_set_int(lc, LEMON_OPT_LW, atoi(value));
		return 1;
	}
	if (equals(name, "ol") || equals(name, "overline"))
	{
		cfg_set_int(lc, LEMON_OPT_OL, equals(value, "true"));
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		cfg_set_int(lc, LEMON_OPT_UL, equals(value, "true"));
		return 1;
	}
	if (equals(name, "h") || equals(name, "height"))
	{
		cfg_set_int(lc, LEMON_OPT_HEIGHT, atoi(value));
		return 1;
	}
	if (equals(name, "w") || equals(name, "width"))
	{
		cfg_set_int(lc, LEMON_OPT_WIDTH, atoi(value));
		return 1;
	}
	if (equals(name, "x"))
	{
		cfg_set_int(lc, LEMON_OPT_X, atoi(value));
		return 1;
	}
	if (equals(name, "y"))
	{
		cfg_set_int(lc, LEMON_OPT_Y, atoi(value));
		return 1;
	}
	if (equals(name, "dock") || equals(name, "position"))
	{
		cfg_set_int(lc, LEMON_OPT_BOTTOM, equals(value, "bottom"));
		return 1;
	}
	if (equals(name, "force"))
	{
		cfg_set_int(lc, LEMON_OPT_FORCE, equals(value, "true"));
		return 1;
	}
	if (equals(name, "offset") || equals(name, "block-offset"))
	{
		cfg_set_int(lc, LEMON_OPT_BLOCK_OFFSET, atoi(value));
		return 1;
	}
	if (equals(name, "prefix") || equals(name, "block-prefix"))
	{
		cfg_set_str(lc, LEMON_OPT_BLOCK_PREFIX, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "suffix") || equals(name, "block-suffix"))
	{
		cfg_set_str(lc, LEMON_OPT_BLOCK_SUFFIX, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "format") || equals(name, "blocks"))
	{
		cfg_set_str(lc, LEMON_OPT_FORMAT, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "font") || equals(name, "block-font"))
	{
		cfg_set_str(lc, LEMON_OPT_BLOCK_FONT, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "label-font"))
	{
		cfg_set_str(lc, LEMON_OPT_LABEL_FONT, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "affix-font"))
	{
		cfg_set_str(lc, LEMON_OPT_AFFIX_FONT, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "block-bg") || equals(name, "block-background"))
	{
		cfg_set_str(lc, LEMON_OPT_BLOCK_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "label-bg") || equals(name, "label-background"))
	{
		cfg_set_str(lc, LEMON_OPT_LABEL_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "label-fg") || equals(name, "label-foreground"))
	{
		cfg_set_str(lc, LEMON_OPT_LABEL_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "affix-bg") || equals(name, "affix-background"))
	{
		cfg_set_str(lc, LEMON_OPT_AFFIX_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "affix-fg") || equals(name, "affix-foreground"))
	{
		cfg_set_str(lc, LEMON_OPT_AFFIX_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}

	// Unknown section or name
	return 0;
}

int block_ini_handler(void *data, const char *section, const char *name, const char *value)
{
	// Unpack the data
	thing_s *block = (thing_s *) data;
	cfg_s *bc = &block->cfg;

	// Check the `name` and do the thing
	if (equals(name, "bin") || equals(name, "cmd") || equals(name, "command"))
	{
		cfg_set_str(bc, BLOCK_OPT_BIN, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		cfg_set_str(bc, BLOCK_OPT_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		cfg_set_str(bc, BLOCK_OPT_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "label-bg") || equals(name, "label-background"))
	{
		cfg_set_str(bc, BLOCK_OPT_LABEL_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "label-fg") || equals(name, "label-foreground"))
	{
		cfg_set_str(bc, BLOCK_OPT_LABEL_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "affix-bg") || equals(name, "affix-background"))
	{
		cfg_set_str(bc, BLOCK_OPT_AFFIX_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "affix-fg") || equals(name, "affix-foreground"))
	{
		cfg_set_str(bc, BLOCK_OPT_AFFIX_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "lc") || equals(name, "line"))
	{
		cfg_set_str(bc, BLOCK_OPT_LC, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "ol") || equals(name, "overline"))
	{
		cfg_set_int(bc, BLOCK_OPT_OL, equals(value, "true"));
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		cfg_set_int(bc, BLOCK_OPT_UL, equals(value, "true"));
		return 1;
	}
	if (equals(name, "pad") || equals(name, "padding") || equals(name, "width"))
	{
		cfg_set_int(bc, BLOCK_OPT_WIDTH, atoi(value));
		return 1;
	}
	if (equals(name, "offset"))
	{
		cfg_set_int(bc, BLOCK_OPT_OFFSET, atoi(value));
		return 1;
	}
	if (equals(name, "prefix"))
	{
		cfg_set_str(bc, BLOCK_OPT_PREFIX, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "suffix"))
	{
		cfg_set_str(bc, BLOCK_OPT_SUFFIX, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "label"))
	{
		cfg_set_str(bc, BLOCK_OPT_LABEL, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "unit"))
	{
		cfg_set_str(bc, BLOCK_OPT_UNIT, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "reload"))
	{
		if (is_quoted(value)) // String means trigger!
		{
			block->b_type = BLOCK_SPARKED;
			cfg_set_str(bc, BLOCK_OPT_TRIGGER, unquote(value));
			cfg_set_float(bc, BLOCK_OPT_RELOAD, 0.0);
		}
		else
		{
			block->b_type = BLOCK_TIMED;
			cfg_set_float(bc, BLOCK_OPT_RELOAD, atof(value));
		}
		return 1;
	}
	if (equals(name, "trigger"))
	{
		block->b_type = BLOCK_SPARKED;
		cfg_set_str(bc, BLOCK_OPT_TRIGGER, is_quoted(value) ? unquote(value) : strdup(value));
		cfg_set_float(bc, BLOCK_OPT_RELOAD, 0.0);
		return 1;
	}
	if (equals(name, "consume"))
	{
		cfg_set_int(bc, BLOCK_OPT_CONSUME, equals(value, "true"));
		return 1;
	}
	if (equals(name, "live"))
	{
		block->b_type = BLOCK_LIVE;
		cfg_set_int(bc, BLOCK_OPT_LIVE, equals(value, "true"));
		return 1;
	}
	if (equals(name, "mouse-left"))
	{
		cfg_set_str(bc, BLOCK_OPT_CMD_LMB, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "mouse-middle"))
	{
		cfg_set_str(bc, BLOCK_OPT_CMD_MMB, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "mouse-right"))
	{
		cfg_set_str(bc, BLOCK_OPT_CMD_RMB, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "scroll-up"))
	{
		cfg_set_str(bc, BLOCK_OPT_CMD_SUP, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "scroll-down"))
	{
		cfg_set_str(bc, BLOCK_OPT_CMD_SDN, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}

	// Unknown section or name
	return 0;
}

