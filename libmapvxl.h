#ifndef LIBMAPVXL_H
#define LIBMAPVXL_H

#define MAP_X_MAX 512
#define MAP_Y_MAX MAP_X_MAX
#define MAP_Z_MAX 64
#define DEFAULT_COLOR 0xFF674028

typedef struct {
    unsigned char blocks[MAP_X_MAX][MAP_Y_MAX][MAP_Z_MAX];
    unsigned int color[MAP_X_MAX][MAP_Y_MAX][MAP_Z_MAX];
} MapVxl;

void mapvxlLoadVXL(MapVxl *map, unsigned char *v);
unsigned char mapvxlIsSurface(MapVxl *map, int x, int y, int z);
void mapvxlSetColor(MapVxl *map, int x, int y, int z, unsigned int c)
void mapvxlWriteMap(MapVxl *map, unsigned char *mapOut);
unsigned int mapvxlGetColor(MapVxl *map, int x, int y, int z);
void mapvxlSetAir(MapVxl *map, int x, int y, int z);
unsigned char mapvxlIsSolid(MapVxl *map, int x, int y, int z);

#endif
