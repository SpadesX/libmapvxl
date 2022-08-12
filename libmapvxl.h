#ifndef LIBMAPVXL_H
#define LIBMAPVXL_H

#include <stddef.h>

#define DEFAULT_COLOR 0xFF674028

typedef struct {
	unsigned short MAP_X_MAX;
	unsigned short MAP_Y_MAX;
	unsigned short MAP_Z_MAX;
	unsigned char ***blocks;
	unsigned int ***color;
} MapVxl;

void mapvxlCreate(MapVxl *map, int maxX, int maxY, int maxZ);
void mapvxlFree(MapVxl *map);
void mapvxlLoadVXL(MapVxl *map, unsigned char *v);
size_t mapvxlWriteMap(MapVxl *map, unsigned char *mapOut);

unsigned char mapvxlFindTopBlock(MapVxl *map, int x, int y);

void mapvxlSetColor(MapVxl *map, int x, int y, int z, unsigned int c);
unsigned int mapvxlGetColor(MapVxl *map, int x, int y, int z);

void mapvxlSetAir(MapVxl *map, int x, int y, int z);
unsigned char mapvxlIsSurface(MapVxl *map, int x, int y, int z);
unsigned char mapvxlIsSolid(MapVxl *map, int x, int y, int z);

#endif
