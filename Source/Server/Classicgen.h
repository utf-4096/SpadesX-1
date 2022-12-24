#include <libmapvxl.h>
#include <math.h>
#include <Util/Types.h>

#ifndef CLASSICGEN_H
#define CLASSICGEN_H

typedef struct classicgen_opt {
	color_t color_ground;
	color_t color_grass1;
	color_t color_grass2;
	color_t color_water;
	int seed;
} classicgen_opt_t;

void genland(classicgen_opt_t options, mapvxl_t* map);


#endif