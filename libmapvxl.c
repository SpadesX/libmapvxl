#include "libmapvxl.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static inline void _mapvxl_unset_block(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z)
{
    if (x > map->size_x || y > map->size_y || z > map->size_z) {
        printf("LIBMAPVXL ERROR: Coordinates X: %d Y: %d Z: %d are out of bounds during block clearing\n", x, y, z);
        assert(1);
    }
    map->blocks[x][y][z].data &= ~0x01;
}

static inline int _mapvxl_is_solid(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z)
{
    return map->blocks[x][y][z].data & 0x01;
}

static inline uint8_t* _mapvxl_write_block_color(uint8_t* out, mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z)
{
    mapvxl_block_t* block = &map->blocks[x][y][z];
    // assume color is ARGB native, but endianness is unknown
    // file format endianness is ARGB little endian, i.e. B,G,R,A
    *(out++) = block->b;
    *(out++) = block->g;
    *(out++) = block->r;
    *(out++) = 0xFF;
    return out;
}

void mapvxl_create(mapvxl_t* map, uint16_t size_x, uint16_t size_y, uint16_t size_z)
{
    assert(size_x > 0 && size_x < 0xFFFF);
    assert(size_y > 0 && size_y < 0xFFFF);
    assert(size_z > 0 && size_z < 0xFFFF);

    map->size_x = size_x;
    map->size_y = size_y;
    map->size_z = size_z;

    uint32_t size_xy  = size_x * size_y;
    uint32_t size_xyz = size_xy * size_z;

    map->memory_blocks_pp = (mapvxl_block_t***)calloc(size_x, sizeof(mapvxl_block_t**));
    map->memory_blocks_p  = (mapvxl_block_t**)calloc(size_xy, sizeof(mapvxl_block_t*));
    map->memory_blocks    = (mapvxl_block_t*)calloc(size_xyz, sizeof(mapvxl_block_t));

    for (uint16_t i = 0; i < size_x; ++i) {
        map->memory_blocks_pp[i] = map->memory_blocks_p + (i * size_y);
    }

    for (uint32_t i = 0; i < size_xy; ++i) {
        map->memory_blocks_p[i] = map->memory_blocks + (i * size_z);
    }

    map->blocks = map->memory_blocks_pp;
}

void mapvxl_free(mapvxl_t* map)
{
    free(map->memory_blocks_pp);
    free(map->memory_blocks_p);
    free(map->memory_blocks);
}

static inline uint8_t* _mapvxl_read_column(mapvxl_t* map, uint8_t* data, uint16_t x, uint16_t y)
{
    for (uint32_t z = 0;;) {
        uint8_t span_size = data[0]; // N
        uint8_t top_start = data[1]; // S
        uint8_t top_end   = data[2]; // E
        // mapvxl_byte_t air_start = data[3]; // A - unused

        // air
        for (; z < top_start; ++z) {
            _mapvxl_unset_block(map, x, y, z);
        }

        // number of top blocks
        uint8_t top_length = top_end - top_start + 1; // K = S - E + 1

        // top
        const uint32_t* colors = (const uint32_t*)(data + 4);
        for (; z <= top_end; ++z, ++colors) {
            mapvxl_set_color(map, x, y, z, *colors);
        }

        if (span_size == 0) { // last span in column
            data += 4 * (top_length + 1);
            break;
        }

        // number of bottom blocks
        uint8_t bottom_length = (span_size - 1) - top_length; // Z = (N - 1) - K
        // move to the next span
        data += span_size * 4;
        // bottom ends where air begins

        uint8_t bottom_end   = data[3]; // (M - 1) - block end, M - next span air
        uint8_t bottom_start = bottom_end - bottom_length; // M -  Z

        // bottom
        for (z = bottom_start; z < bottom_end; ++z, ++colors) {
            mapvxl_set_color(map, x, y, z, *colors);
        }
    }
    return data;
}

void mapvxl_read(mapvxl_t* map, uint8_t* src)
{
    const unsigned int BLOCK_CLEAR = 0x01000000 | (MAPVXL_DEFAULT_COLOR & 0x00FFFFFF);
    for (uint16_t y = 0; y < map->size_y; ++y) {
        for (uint16_t x = 0; x < map->size_x; ++x) {
            for (uint16_t z = 0; z < map->size_z; ++z) {
                map->blocks[x][y][z].raw = BLOCK_CLEAR;
            }
            src = _mapvxl_read_column(map, src, x, y);
        }
    }
}

static inline uint8_t* _mapvxl_write_column(mapvxl_t* map, uint8_t* data, uint16_t i, uint16_t j)
{
    for (uint16_t k = 0; k < map->size_z;) {
        // find the air region
        uint8_t air_start = k;
        while (k < map->size_z && !_mapvxl_is_solid(map, i, j, k)) {
            ++k;
        }

        // find the top region
        uint8_t top_start = k;
        while (k < map->size_z && mapvxl_is_surface(map, i, j, k)) {
            ++k;
        }
        uint8_t top_end = k;

        // now skip past the solid voxels
        while (k < map->size_z && _mapvxl_is_solid(map, i, j, k) && !mapvxl_is_surface(map, i, j, k)) {
            ++k;
        }

        // at the end of the solid voxels, we have colored voxels.
        // in the "normal" case they're bottom colors; but it's
        // possible to have air-color-solid-color-solid-color-air,
        // which we encode as air-color-solid-0, 0-color-solid-air

        // so figure out if we have any bottom colors at this point
        uint16_t z = k;

        uint8_t bottom_start = k;
        while (z < map->size_z && mapvxl_is_surface(map, i, j, z)) {
            ++z;
        }
        if (z != map->size_z) {
            // these are real bottom colors so we can write them
            // otherwise, the bottom colors of this span are empty, because we'l
            // emit as top colors
            while (mapvxl_is_surface(map, i, j, k)) {
                ++k;
            }
        }
        uint8_t bottom_end = k;

        // now we're ready to write a span
        uint8_t top_length    = top_end - top_start;
        uint8_t bottom_length = bottom_end - bottom_start;
        uint8_t colors_length = top_length + bottom_length;

        // span size
        if (k == map->size_z) {
            *(data++) = 0;
        } else {
            *(data++) = colors_length + 1;
        }

        *(data++) = top_start;
        *(data++) = top_end - 1;
        *(data++) = air_start;

        for (z = 0; z < top_length; ++z) {
            data = _mapvxl_write_block_color(data, map, i, j, top_start + z);
        }
        for (z = 0; z < bottom_length; ++z) {
            data = _mapvxl_write_block_color(data, map, i, j, bottom_start + z);
        }
    }
    return data;
}

size_t mapvxl_write(mapvxl_t* map, uint8_t* out)
{
    // The size of mapOut should be the max possible memory size of
    // uncompressed VXL format in memory given the XYZ size
    // which is map->MAP_X_MAX * map->MAP_Y_MAX * (map->MAP_Z_MAX / 2) * 8

    uint8_t* begin = out;
    for (uint16_t j = 0; j < map->size_x; ++j) {
        for (uint16_t i = 0; i < map->size_y; ++i) {
            out = _mapvxl_write_column(map, out, i, j);
        }
    }
    return out - begin;
}

uint8_t mapvxl_find_top_block(mapvxl_t* map, uint16_t x, uint16_t y)
{
    if (x > map->size_x || y > map->size_y) {
        return 0;
    }

    for (uint8_t z = 0; z < map->size_z; ++z) {
        if (map->blocks[x][y][z].data & 0x01) {
            return z;
        }
    }
    return 0;
}

void mapvxl_set_color(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z, uint32_t c)
{
    if (x > map->size_x || y > map->size_y || z > map->size_z) {
        printf("LIBMAPVXL ERROR: Coordinates X: %d Y: %d Z: %d are out of bounds during color set\n", x, y, z);
        assert(1);
    }
    map->blocks[x][y][z].raw = 0x01000000 | (c & 0x00FFFFFF); // Set visible and copy RGB from ARGB
}

uint32_t mapvxl_get_color(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z)
{
    if (x > map->size_x || y > map->size_y || z > map->size_z) {
        printf("LIBMAPVXL ERROR: Coordinates X: %d Y: %d Z: %d are out of bounds during get color\n", x, y, z);
        assert(1);
    }
    if (!_mapvxl_is_solid(map, x, y, z)) {
        return MAPVXL_DEFAULT_COLOR;
    }
    return map->blocks[x][y][z].raw | 0xFF000000;
}

void mapvxl_set_air(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z)
{
    if (x > map->size_x || y > map->size_y || z > map->size_z) {
        printf("LIBMAPVXL ERROR: Coordinates X: %d Y: %d Z: %d are out of bounds during setting air\n", x, y, z);
        assert(1);
    }
    map->blocks[x][y][z].raw = 0x00FFFFFF & MAPVXL_DEFAULT_COLOR;
}

int mapvxl_is_surface(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z)
{
    if (!_mapvxl_is_solid(map, x, y, z)) {
        return 0;
    }
    if (z == 0) {
        return 1;
    }
    if (x > 0 && !_mapvxl_is_solid(map, x - 1, y, z)) {
        return 1;
    }
    if (x + 1 < map->size_x && !_mapvxl_is_solid(map, x + 1, y, z)) {
        return 1;
    }
    if (y > 0 && !_mapvxl_is_solid(map, x, y - 1, z)) {
        return 1;
    }
    if (y + 1 < map->size_y && !_mapvxl_is_solid(map, x, y + 1, z)) {
        return 1;
    }
    if (z > 0 && !_mapvxl_is_solid(map, x, y, z - 1)) {
        return 1;
    }
    if (z + 1 < map->size_z && !_mapvxl_is_solid(map, x, y, z + 1)) {
        return 1;
    }
    return 0;
}

int mapvxl_is_solid(mapvxl_t* map, uint16_t x, uint16_t y, uint16_t z)
{
    if (x > map->size_x || y > map->size_y || z > map->size_z) {
        printf("LIBMAPVXL ERROR: Coordinates X: %d Y: %d Z: %d are out of bounds during solid check\n", x, y, z);
        assert(1);
    }
    return _mapvxl_is_solid(map, x, y, z);
}
