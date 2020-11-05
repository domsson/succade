#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <string.h>    // strdup()
#include "ini.h"       // https://github.com/benhoyt/inih
#include "succade.h"   // defines, structs, all that stuff

int lemon_ini_handler(void *data, const char *section, const char *name, const char *value)
{
	// Unpack the data, which should be a lemon_s (struct succade_lemon)
	thing_s *lemon = (thing_s*) data;
	cfg_s *lc = &lemon->cfg;

	if (equals(name, "name") || equals(name, "wm-name"))
	{
		cfg_set_str(lc, LEMON_OPT_NAME, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "command") || equals(name, "exec") || equals(name, "cmd"))
	{
		cfg_set_str(lc, LEMON_OPT_BIN, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "foreground") || equals(name, "fg"))
	{
		cfg_set_str(lc, LEMON_OPT_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "background") || equals(name, "bg"))
	{
		cfg_set_str(lc, LEMON_OPT_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "line-color") || equals(name, "lc"))
	{
		cfg_set_str(lc, LEMON_OPT_LC, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "line-width") || equals(name, "lw"))
	{
		cfg_set_int(lc, LEMON_OPT_LW, atoi(value));
		return 1;
	}
	if (equals(name, "separator"))
	{
		cfg_set_str(lc, LEMON_OPT_SEPARATOR, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "height") || equals(name, "h"))
	{
		cfg_set_int(lc, LEMON_OPT_HEIGHT, atoi(value));
		return 1;
	}
	if (equals(name, "width") || equals(name, "w"))
	{
		cfg_set_int(lc, LEMON_OPT_WIDTH, atoi(value));
		return 1;
	}
	if (equals(name, "left") || equals(name, "x"))
	{
		cfg_set_int(lc, LEMON_OPT_X, atoi(value));
		return 1;
	}
	if (equals(name, "top") || equals(name, "y"))
	{
		cfg_set_int(lc, LEMON_OPT_Y, atoi(value));
		return 1;
	}
	if (equals(name, "bottom"))
	{
		cfg_set_int(lc, LEMON_OPT_BOTTOM, equals(value, "true"));
		return 1;
	}
	if (equals(name, "dock") || equals(name, "position")) // alternative to 'bottom'
	{
		cfg_set_int(lc, LEMON_OPT_BOTTOM, equals(value, "bottom"));
		return 1;
	}
	if (equals(name, "force"))
	{
		cfg_set_int(lc, LEMON_OPT_FORCE, equals(value, "true"));
		return 1;
	}
	if (equals(name, "areas"))
	{
		cfg_set_int(lc, LEMON_OPT_AREAS, atoi(value));
		return 1;
	}
	if (equals(name, "blocks") || equals(name, "format"))
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

	// Unknown section or name
	return 0;
}

int block_ini_handler(void *data, const char *section, const char *name, const char *value)
{
	// Unpack the data
	thing_s *block = (thing_s *) data;
	cfg_s *bc = &block->cfg;

	// Check the `name` and do the thing
	if (equals(name, "command") || equals(name, "exec") || equals(name, "cmd"))
	{
		cfg_set_str(bc, BLOCK_OPT_BIN, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "foreground") || equals(name, "fg") || equals(name, "block-foreground") || equals(name, "block-fg"))
	{
		cfg_set_str(bc, BLOCK_OPT_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "background") || equals(name, "bg") || equals(name, "block-background") || equals(name, "block-bg"))
	{
		cfg_set_str(bc, BLOCK_OPT_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "label-foreground") || equals(name, "label-fg"))
	{
		cfg_set_str(bc, BLOCK_OPT_LABEL_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "label-background") || equals(name, "label-bg"))
	{
		cfg_set_str(bc, BLOCK_OPT_LABEL_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "affix-foreground") || equals(name, "affix-fg"))
	{
		cfg_set_str(bc, BLOCK_OPT_AFFIX_FG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "affix-background") || equals(name, "affix-bg"))
	{
		cfg_set_str(bc, BLOCK_OPT_AFFIX_BG, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "line-color") || equals(name, "line") || equals(name, "lc"))
	{
		cfg_set_str(bc, BLOCK_OPT_LC, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "overline") || equals(name, "ol"))
	{
		cfg_set_int(bc, BLOCK_OPT_OL, equals(value, "true"));
		return 1;
	}
	if (equals(name, "underline") || equals(name, "ul"))
	{
		cfg_set_int(bc, BLOCK_OPT_UL, equals(value, "true"));
		return 1;
	}
	if (equals(name, "min-width") || equals(name, "left-pad"))
	{
		cfg_set_int(bc, BLOCK_OPT_MIN_WIDTH, atoi(value));
		return 1;
	}
	if (equals(name, "margin"))
	{
		cfg_set_int(bc, BLOCK_OPT_MARGIN_LEFT, atoi(value));
		cfg_set_int(bc, BLOCK_OPT_MARGIN_RIGHT, atoi(value));
		return 1;
	}
	if (equals(name, "margin-left"))
	{
		cfg_set_int(bc, BLOCK_OPT_MARGIN_LEFT, atoi(value));
		return 1;
	}
	if (equals(name, "margin-right"))
	{
		cfg_set_int(bc, BLOCK_OPT_MARGIN_RIGHT, atoi(value));
		return 1;
	}
	if (equals(name, "padding"))
	{
		cfg_set_int(bc, BLOCK_OPT_PADDING_LEFT, atoi(value));
		cfg_set_int(bc, BLOCK_OPT_PADDING_RIGHT, atoi(value));
		return 1;
	}
	if (equals(name, "padding-left"))
	{
		cfg_set_int(bc, BLOCK_OPT_PADDING_LEFT, atoi(value));
		return 1;
	}
	if (equals(name, "padding-right"))
	{
		cfg_set_int(bc, BLOCK_OPT_PADDING_RIGHT, atoi(value));
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
	if (equals(name, "interval") || equals(name, "reload"))
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
	if (equals(name, "raw"))
	{
		cfg_set_int(bc, BLOCK_OPT_RAW, equals(value, "true"));
		return 1;
	}
	if (equals(name, "mouse-left") || equals(name, "click-left"))
	{
		cfg_set_str(bc, BLOCK_OPT_CMD_LMB, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "mouse-middle") || equals(name, "click-middle"))
	{
		cfg_set_str(bc, BLOCK_OPT_CMD_MMB, is_quoted(value) ? unquote(value) : strdup(value));
		return 1;
	}
	if (equals(name, "mouse-right") || equals(name, "click-right"))
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

