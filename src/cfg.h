#ifndef CFG_H
#define CFG_H

#include <stdlib.h>    // NULL, size_t
#include <string.h>    // strdup()

enum cfg_opt_type
{
	OPT_TYPE_NONE,
	OPT_TYPE_INT,
	OPT_TYPE_FLOAT,
	OPT_TYPE_STRING
};

typedef enum cfg_opt_type cfg_opt_type_e;

union cfg_opt
{
	int    i;
	float  f;
	char  *s;
};

typedef union cfg_opt cfg_opt_u;

struct cfg {
	char           *name;
	cfg_opt_u      *opts;
	cfg_opt_type_e *type;
	size_t          size;
};

typedef struct cfg cfg_s;

#ifdef CFG_IMPLEMENTATION

cfg_s *cfg_init(cfg_s *cfg, const char *name, size_t size)
{
	cfg->name = strdup(name);
	cfg->size = size;
	cfg->opts = malloc(size * sizeof(cfg_opt_u));
	cfg->type = malloc(size * sizeof(cfg_opt_type_e));

	for (size_t i = 0; i < size; ++i)
	{
		cfg->opts[i] = (cfg_opt_u) { 0 };
		cfg->type[i] = OPT_TYPE_NONE;
	}

	return cfg;
}

void cfg_free(cfg_s *cfg)
{
	for (size_t i = 0; i < cfg->size; ++i)
	{
		if (cfg->type[i] == OPT_TYPE_STRING)
		{
			free(cfg->opts[i].s);
		}	
	}

	free(cfg->name);
	free(cfg->opts);
	free(cfg->type);
	cfg->name = NULL;
	cfg->opts = NULL;
	cfg->type = NULL;
}

int cfg_has(const cfg_s *cfg, size_t idx)
{
	return (idx < cfg->size && cfg->type[idx] != 0);
}

cfg_opt_type_e cfg_type(const cfg_s *cfg, size_t idx)
{
	return cfg->type[idx];
}

void cfg_set_int(const cfg_s *cfg, size_t idx, int val)
{
	if (idx >= cfg->size)
		return;

	cfg->opts[idx].i = val;
	cfg->type[idx] = OPT_TYPE_INT;
}

void cfg_set_float(cfg_s *cfg, size_t idx, float val)
{
	if (idx >= cfg->size)
		return;

	cfg->opts[idx].f = val;
	cfg->type[idx] = OPT_TYPE_FLOAT;
}

void cfg_set_str(cfg_s *cfg, size_t idx, char *val)
{
	if (idx >= cfg->size)
		return;

	cfg->opts[idx].s = val;
	cfg->type[idx] = OPT_TYPE_STRING;
}

cfg_opt_u *cfg_get(cfg_s *cfg, size_t idx)
{
	return cfg_has(cfg, idx) ? &cfg->opts[idx] : NULL;
}

int cfg_get_int(const cfg_s *cfg, size_t idx)
{
	return (idx < cfg->size && cfg->type[idx] == OPT_TYPE_INT) ?
		cfg->opts[idx].i : 0;
}

float cfg_get_float(const cfg_s *cfg, size_t idx)
{
	return (idx < cfg->size && cfg->type[idx] == OPT_TYPE_FLOAT) ?
		cfg->opts[idx].f : 0.0;
}

char *cfg_get_str(const cfg_s *cfg, size_t idx)
{
	return (idx < cfg->size && cfg->type[idx] == OPT_TYPE_STRING) ?
		cfg->opts[idx].s : NULL;
}

#endif /* CFG_IMPLEMENTATION */
#endif /* CFG_H */
