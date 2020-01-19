#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <string.h>    // strlen(), strcmp(), ...
#include <float.h>     // DBL_MAX
#include "ini.h"       // https://github.com/benhoyt/inih
#include "succade.h"   // defines, structs, all that stuff

int lemon_ini_handler(void *data, const char *section, const char *name, const char *value)
{
	// Unpack the data, which should be a scd_lemon_s (struct succade_lemon)
	scd_lemon_s *bar = (scd_lemon_s*) data;

	// Check for `name` and set the appropriate property
	if (equals(name, "name"))
	{
		bar->name = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bin"))
	{
		bar->bin = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		bar->fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		bar->bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "lc") || equals(name, "line"))
	{
		bar->lc = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "line-width"))
	{
		bar->lw = atoi(value);
		return 1;
	}
	if (equals(name, "ol") || equals(name, "overline"))
	{
		bar->ol = equals(value, "true");
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		bar->ul = equals(value, "true");
		return 1;
	}
	if (equals(name, "h") || equals(name, "height"))
	{
		bar->h = atoi(value);
		return 1;
	}
	if (equals(name, "w") || equals(name, "width"))
	{
		bar->w = atoi(value);
		return 1;
	}
	if (equals(name, "x"))
	{
		bar->x = atoi(value);
		return 1;
	}
	if (equals(name, "y"))
	{
		bar->y = atoi(value);
		return 1;
	}
	if (equals(name, "dock"))
	{
		bar->bottom = equals(value, "bottom");
		return 1;
	}
	if (equals(name, "force"))
	{
		bar->force = equals(value, "true");
		return 1;
	}
	if (equals(name, "offset") || equals(name, "block-offset"))
	{
		bar->offset = atoi(value);
		return 1;
	}
	if (equals(name, "prefix") || equals(name, "block-prefix"))
	{
		bar->prefix = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "suffix") || equals(name, "block-suffix"))
	{
		bar->suffix = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "format") || equals(name, "blocks"))
	{
		bar->format = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "font") || equals(name, "block-font"))
	{
		bar->block_font = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-font"))
	{
		bar->label_font = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-font"))
	{
		bar->affix_font = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "block-background") || equals(name, "block-bg"))
	{
		bar->block_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-background") || equals(name, "label-bg"))
	{
		bar->label_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-foreground") || equals(name, "label-fg"))
	{
		bar->label_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-background") || equals(name, "affix-bg"))
	{
		bar->affix_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-foreground") || equals(name, "affix-fg"))
	{
		bar->affix_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}

	// Unknown section or name
	return 0;
}

int block_ini_handler(void *data, const char *section, const char *name, const char *value)
{
	// Unpack the data
	scd_block_s *block = (scd_block_s*) data;

	// Check the `name` and do the thing
	if (equals(name, "bin"))
	{
		block->bin = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		block->fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		block->bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-background") || equals(name, "label-bg"))
	{
		block->label_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-foreground") || equals(name, "label-fg"))
	{
		block->label_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-background") || equals(name, "affix-bg"))
	{
		block->affix_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-foreground") || equals(name, "affix-fg"))
	{
		block->affix_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "lc") || equals(name, "line"))
	{
		block->lc = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "ol") || equals(name, "overline"))
	{
		block->ol = equals(value, "true");
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		block->ul = equals(value, "true");
		return 1;
	}
	if (equals(name, "live"))
	{
		block->live = equals(value, "true");
		return 1;
	}
	if (equals(name, "pad") || equals(name, "padding"))
	{
		block->padding = atoi(value);
		return 1;
	}
	if (equals(name, "offset"))
	{
		block->offset = atoi(value);
		return 1;
	}
	if (equals(name, "label"))
	{
		block->label = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "reload"))
	{
		if (is_quoted(value)) // String means trigger!
		{
			block->trigger = unquote(value);
			block->reload = 0.0;
		}
		else
		{
			block->reload = atof(value);
		}
		return 1;
	}
	if (equals(name, "trigger"))
	{
		block->trigger = is_quoted(value) ? unquote(value) : strdup(value);
		block->reload = 0.0;
		return 1;
	}
	if (equals(name, "mouse-left"))
	{
		block->cmd_lmb = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "mouse-middle"))
	{
		block->cmd_mmb = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "mouse-right"))
	{
		block->cmd_rmb = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "scroll-up"))
	{
		block->cmd_sup = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "scroll-down"))
	{
		block->cmd_sdn = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}

	// Unknown section or name
	return 0;
}

