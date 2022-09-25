#ifndef LIBMAPVXL_H
#define LIBMAPVXL_H

#include <stddef.h>
#include <stdint.h>

#define MAPVXL_DEFAULT_COLOR 0xFF674028

typedef union mapvxl_block {
    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t data;
    };
    uint32_t raw;
} mapvxl_block_t;

_Static_assert(sizeof(mapvxl_block_t) == 4, "invalid size");

typedef struct mapvxl {
    uint16_t          size_x;
    uint16_t          size_y;
    uint16_t          size_z;
    mapvxl_block_t*** blocks;

    // flat buffers
    mapvxl_block_t*** memory_blocks_pp;
    mapvxl_block_t**  memory_blocks_p;
    mapvxl_block_t*   memory_blocks;
} mapvxl_t;

void     mapvxl_create(mapvxl_t* map, uint16_t size_x, uint16_t size_y, uint16_t size_z);
void     mapvxl_free(mapvxl_t* map);
void     mapvxl_read(mapvxl_t* map, uint8_t* src);
size_t   mapvxl_write(mapvxl_t* map, uint8_t* out);
uint8_t  mapvxl_find_top_block(mapvxl_t* map, uint16_t x, uint16_t y);
void     mapvxl_set_color(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z, uint32_t c);
uint32_t mapvxl_get_color(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z);
void     mapvxl_set_air(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z);
int      mapvxl_is_surface(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z);
int      mapvxl_is_solid(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z);

#endif
