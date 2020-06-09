#include <stdlib.h>    // NULL, size_t
#include <string.h>    // strdup()
#include <limits.h>    // INT_MIN
#include <float.h>     // FLT_MIN

enum opt_type
{
	OPT_TYPE_NONE,
	OPT_TYPE_INT,
	OPT_TYPE_FLOAT,
	OPT_TYPE_STRING
};

typedef enum opt_type opt_type_e;

union cfg_opt
{
	int    i;
	float  f;
	char  *s;
};

typedef union cfg_opt cfg_opt_u;

struct cfg {
	char          *name;
	union cfg_opt *opts;
	enum opt_type *type;
	size_t         size;
};

typedef struct cfg cfg_s;

struct cfg *cfg_init(struct cfg *cfg, const char *name, size_t size)
{
	cfg->name = strdup(name);
	cfg->size = size;
	cfg->opts = malloc(size * sizeof(struct cfg));
	cfg->type = malloc(size * sizeof(enum opt_type));

	for (size_t i = 0; i < size; ++i)
	{
		cfg->opts[i] = (union cfg_opt) { 0 };
		cfg->type[i] = OPT_TYPE_NONE;
	}

	return cfg;
}

void cfg_free(struct cfg *cfg)
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

int cfg_has(struct cfg *cfg, size_t idx)
{
	return (idx < cfg->size && cfg->type[idx]);
}

void cfg_set_int(struct cfg *cfg, size_t idx, int val)
{
	if (idx >= cfg->size)
		return;

	cfg->opts[idx].i = val;
	cfg->type[idx] = OPT_TYPE_INT;
}

void cfg_set_float(struct cfg *cfg, size_t idx, float val)
{
	if (idx >= cfg->size)
		return;

	cfg->opts[idx].f = val;
	cfg->type[idx] = OPT_TYPE_FLOAT;
}

void cfg_set_str(struct cfg *cfg, size_t idx, char *val)
{
	if (idx >= cfg->size)
		return;

	cfg->opts[idx].s = val;
	cfg->type[idx] = OPT_TYPE_STRING;
}

union cfg_opt *cfg_get(struct cfg *cfg, size_t idx)
{
	return cfg_has(cfg, idx) ? &cfg->opts[idx] : NULL;
}

int cfg_get_int(struct cfg *cfg, size_t idx)
{
	return (idx < cfg->size && cfg->type[idx] == OPT_TYPE_INT) ?
		cfg->opts[idx].i : INT_MIN;
}

float cfg_get_float(struct cfg *cfg, size_t idx)
{
	return (idx < cfg->size && cfg->type[idx] == OPT_TYPE_FLOAT) ?
		cfg->opts[idx].i : FLT_MIN;
}

char *cfg_get_str(struct cfg *cfg, size_t idx)
{
	return (idx < cfg->size && cfg->type[idx] == OPT_TYPE_STRING) ?
		cfg->opts[idx].s : NULL;
}

