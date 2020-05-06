#include <stdlib.h>    // NULL, size_t, EXIT_SUCCESS, EXIT_FAILURE, ...
#include <string.h>    // strlen(), strcmp(), ...
#include <float.h>     // DBL_MAX
#include "ini.h"       // https://github.com/benhoyt/inih
#include "succade.h"   // defines, structs, all that stuff

int lemon_ini_handler(void *data, const char *section, const char *name, const char *value)
{
	// Unpack the data, which should be a lemon_s (struct succade_lemon)
	lemon_s *lemon = (lemon_s*) data;
	lemon_cfg_s *lc = &lemon->lemon_cfg;
	block_cfg_s *bc = &lemon->block_cfg;

	// Check for `name` and set the appropriate property
	if (equals(name, "name"))
	{
		lc->name = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bin") || equals(name, "cmd") || equals(name, "command"))
	{
		lc->bin = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		bc->fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		lc->bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "lc") || equals(name, "line"))
	{
		bc->lc = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "lw") || equals(name, "line-width"))
	{
		lc->lw = atoi(value);
		return 1;
	}
	if (equals(name, "ol") || equals(name, "overline"))
	{
		bc->ol = equals(value, "true");
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		bc->ul = equals(value, "true");
		return 1;
	}
	if (equals(name, "h") || equals(name, "height"))
	{
		lc->h = atoi(value);
		return 1;
	}
	if (equals(name, "w") || equals(name, "width"))
	{
		lc->w = atoi(value);
		return 1;
	}
	if (equals(name, "x"))
	{
		lc->x = atoi(value);
		return 1;
	}
	if (equals(name, "y"))
	{
		lc->y = atoi(value);
		return 1;
	}
	if (equals(name, "dock"))
	{
		lc->bottom = equals(value, "bottom");
		return 1;
	}
	if (equals(name, "force"))
	{
		lc->force = equals(value, "true");
		return 1;
	}
	if (equals(name, "offset") || equals(name, "block-offset"))
	{
		bc->offset = atoi(value);
		return 1;
	}
	if (equals(name, "prefix") || equals(name, "block-prefix"))
	{
		bc->prefix = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "suffix") || equals(name, "block-suffix"))
	{
		bc->suffix = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "format") || equals(name, "blocks"))
	{
		lc->format = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "font") || equals(name, "block-font"))
	{
		lc->block_font = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-font"))
	{
		lc->label_font = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-font"))
	{
		lc->affix_font = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "block-bg") || equals(name, "block-background"))
	{
		bc->bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-bg") || equals(name, "label-background"))
	{
		bc->label_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-fg") || equals(name, "label-foreground"))
	{
		bc->label_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-bg") || equals(name, "affix-background"))
	{
		bc->affix_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-fg") || equals(name, "affix-foreground"))
	{
		bc->affix_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}

	// Unknown section or name
	return 0;
}

int block_ini_handler(void *data, const char *section, const char *name, const char *value)
{
	// Unpack the data
	block_s *block = (block_s*) data;
	block_cfg_s *bc = &block->block_cfg;
	click_cfg_s *cc = &block->click_cfg;

	// Check the `name` and do the thing
	if (equals(name, "bin") || equals(name, "cmd") || equals(name, "command"))
	{
		bc->bin = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "fg") || equals(name, "foreground"))
	{
		bc->fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "bg") || equals(name, "background"))
	{
		bc->bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-bg") || equals(name, "label-background"))
	{
		bc->label_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label-fg") || equals(name, "label-foreground"))
	{
		bc->label_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-bg") || equals(name, "affix-background"))
	{
		bc->affix_bg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "affix-fg") || equals(name, "affix-foreground"))
	{
		bc->affix_fg = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "lc") || equals(name, "line"))
	{
		bc->lc = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "ol") || equals(name, "overline"))
	{
		bc->ol = equals(value, "true");
		return 1;
	}
	if (equals(name, "ul") || equals(name, "underline"))
	{
		bc->ul = equals(value, "true");
		return 1;
	}
	if (equals(name, "pad") || equals(name, "padding") || equals(name, "width"))
	{
		bc->width = atoi(value);
		return 1;
	}
	if (equals(name, "offset"))
	{
		bc->offset = atoi(value);
		return 1;
	}
	if (equals(name, "prefix"))
	{
		bc->prefix = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "suffix"))
	{
		bc->suffix = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "label"))
	{
		bc->label = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "unit"))
	{
		bc->unit = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "reload"))
	{
		if (is_quoted(value)) // String means trigger!
		{
			block->type = BLOCK_SPARKED;
			bc->trigger = unquote(value);
			bc->reload = 0.0;
		}
		else
		{
			block->type = BLOCK_TIMED;
			bc->reload = atof(value);
		}
		return 1;
	}
	if (equals(name, "trigger"))
	{
		block->type = BLOCK_SPARKED;
		bc->trigger = is_quoted(value) ? unquote(value) : strdup(value);
		bc->reload = 0.0;
		return 1;
	}
	if (equals(name, "consume"))
	{
		bc->consume = equals(value, "true");
		return 1;
	}
	if (equals(name, "live"))
	{
		block->type = BLOCK_LIVE;
		bc->live = equals(value, "true");
		return 1;
	}
	if (equals(name, "mouse-left"))
	{
		cc->lmb = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "mouse-middle"))
	{
		cc->mmb = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "mouse-right"))
	{
		cc->rmb = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "scroll-up"))
	{
		cc->sup = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}
	if (equals(name, "scroll-down"))
	{
		cc->sdn = is_quoted(value) ? unquote(value) : strdup(value);
		return 1;
	}

	// Unknown section or name
	return 0;
}

