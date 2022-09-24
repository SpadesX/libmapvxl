#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "libmapvxl.h"


void mapvxlCreate(MapVxl *map, int maxX, int maxY, int maxZ) {
   map->MAP_X_MAX = maxX;
   map->MAP_Y_MAX = maxY;
   map->MAP_Z_MAX = maxZ;
   
   unsigned int maxXY = maxX * maxY;
   unsigned int maxXYZ = maxX * maxY * maxZ;

   map->memory_blocks_pp = (Block***)calloc(maxX, sizeof(Block**));
   map->memory_blocks_p = (Block**)calloc(maxXY, sizeof(Block*));
   map->memory_blocks = (Block*)calloc(maxXYZ, sizeof(Block));

   for (unsigned short i = 0; i < maxX; ++i) {
      map->memory_blocks_pp[i] = map->memory_blocks_p + (i * maxY);
   }

   for (unsigned int i = 0; i < maxXY; ++i) {
      map->memory_blocks_p[i] = map->memory_blocks + (i * maxZ);
   }

   map->blocks = map->memory_blocks_pp;
}

void mapvxlFree(MapVxl *map) {
   free(map->memory_blocks_pp);
   free(map->memory_blocks_p);
   free(map->memory_blocks);
}

static void unsetBlock(MapVxl *map, int x, int y, int z) {
   assert(z >= 0 && z < map->MAP_Z_MAX);
   map->blocks[x][y][z].data &= ~0x01; 
}

void mapvxlSetColor(MapVxl *map, int x, int y, int z, unsigned int c) {
   assert(z >= 0 && z < map->MAP_Z_MAX);
   Block* block = &map->blocks[x][y][z];
   block->raw = 0x01000000 | (c & 0x00FFFFFF); // Set visible and copy RGB from ARGB
}

unsigned char mapvxlFindTopBlock(MapVxl *map, int x, int y) {
   for (int z = 0; z <= 63; ++z) {
      if (map->blocks[x][y][z].data & 0x01) {
         return z;
      }
   }
   return 0;
}

void mapvxlLoadVXL(MapVxl *map, unsigned char *v) {
    int x,y,z;
    const unsigned int BLOCK_CLEAR = 0x01000000 | (DEFAULT_COLOR & 0xFF000000);
    for (y=0; y < map->MAP_Y_MAX; ++y) {
        for (x=0; x < map->MAP_X_MAX; ++x) {
            for (z=0; z < map->MAP_Z_MAX; ++z) {
               map->blocks[x][y][z].raw = BLOCK_CLEAR;
            }
            z = 0;
            while(1) {
                unsigned int *color;
                int i;
                int number_4byte_chunks = v[0];
                int top_color_start = v[1];
                int top_color_end   = v[2]; // inclusive
                int bottom_color_start;
                int bottom_color_end; // exclusive
                int len_top;
                int len_bottom;

                for(i=z; i < top_color_start; i++)
                    unsetBlock(map, x, y, i);

                color = (unsigned int *) (v+4);
                for(z=top_color_start; z <= top_color_end; z++)
                    mapvxlSetColor(map, x,y,z,*color++);

                len_bottom = top_color_end - top_color_start + 1;

                // check for end of data marker
                if (number_4byte_chunks == 0) {
                    // infer ACTUAL number of 4-byte chunks from the length of the color data
                    v += 4 * (len_bottom + 1);
                    break;
                }

                // infer the number of bottom colors in next span from chunk length
                len_top = (number_4byte_chunks-1) - len_bottom;

                // now skip the v pointer past the data to the beginning of the next span
                v += v[0]*4;

                bottom_color_end   = v[3]; // aka air start
                bottom_color_start = bottom_color_end - len_top;

                for(z=bottom_color_start; z < bottom_color_end; ++z) {
                    mapvxlSetColor(map, x,y,z,*color++);
                }
            }
        }
    }
}

unsigned char mapvxlIsSurface(MapVxl *map, int x, int y, int z) {
   if (!(map->blocks[x][y][z].data & 0x01))
      return 0;
   if (z == 0)
      return 1;
   if (x > 0 && !(map->blocks[x - 1][y][z].data & 0x01))
      return 1;
   if (x + 1 < map->MAP_X_MAX && !(map->blocks[x + 1][y][z].data & 0x01))
      return 1;
   if (y > 0 && !(map->blocks[x][y - 1][z].data & 0x01))
      return 1;
   if (y + 1 < map->MAP_Y_MAX && !(map->blocks[x][y + 1][z].data & 0x01))
      return 1;
   if (z > 0 && !(map->blocks[x][y][z - 1].data & 0x01))
      return 1;
   if (z + 1 < map->MAP_Z_MAX && !(map->blocks[x][y][z + 1].data & 0x01))
      return 1;
   return 0;
}

static void mapvxlWriteColor(unsigned char **mapOut, Block* block) {
   // assume color is ARGB native, but endianness is unknown

   // file format endianness is ARGB little endian, i.e. B,G,R,A
   (*mapOut)[0] = block->b;
   (*mapOut)[1] = block->g;
   (*mapOut)[2] = block->r;
   (*mapOut)[3] = 0xFF;
   *mapOut += 4;
}


size_t mapvxlWriteMap(MapVxl *map, unsigned char *mapOut) {
   int i,j,k;
   
   unsigned char *originalSize = mapOut;
   
   //The size of mapOut should be the max possible memory size of
   //uncompressed VXL format in memory given the XYZ size
   //which is map->MAP_X_MAX * map->MAP_Y_MAX * (map->MAP_Z_MAX / 2) * 8

   for (j=0; j < map->MAP_X_MAX; ++j) {
      for (i=0; i < map->MAP_Y_MAX; ++i) {
         k = 0;
         while (k < map->MAP_Z_MAX) {
            int z;

            int air_start;
            int top_colors_start;
            int top_colors_end; // exclusive
            int bottom_colors_start;
            int bottom_colors_end; // exclusive
            int top_colors_len;
            int bottom_colors_len;
            int colors;

            // find the air region
            air_start = k;
            while (k < map->MAP_Z_MAX && !map->blocks[i][j][k].data)
               ++k;

            // find the top region
            top_colors_start = k;
            while (k < map->MAP_Z_MAX && mapvxlIsSurface(map, i,j,k))
               ++k;
            top_colors_end = k;

            // now skip past the solid voxels
            while (k < map->MAP_Z_MAX && map->blocks[i][j][k].data && !mapvxlIsSurface(map,i,j,k))
               ++k;

            // at the end of the solid voxels, we have colored voxels.
            // in the "normal" case they're bottom colors; but it's
            // possible to have air-color-solid-color-solid-color-air,
            // which we encode as air-color-solid-0, 0-color-solid-air
          
            // so figure out if we have any bottom colors at this point
            bottom_colors_start = k;

            z = k;
            while (z < map->MAP_Z_MAX && mapvxlIsSurface(map, i, j, z))
               ++z;

            if (z == map->MAP_Z_MAX || 0)
               ; // in this case, the bottom colors of this span are empty, because we'l emit as top colors
            else {
               // otherwise, these are real bottom colors so we can write them
               while (mapvxlIsSurface(map, i, j, k))  
                  ++k;
            }
            bottom_colors_end = k;

            // now we're ready to write a span
            top_colors_len    = top_colors_end    - top_colors_start;
            bottom_colors_len = bottom_colors_end - bottom_colors_start;

            colors = top_colors_len + bottom_colors_len;

            if (k == map->MAP_Z_MAX) {
               *mapOut = 0;
               mapOut++;
            }
            else{
               *mapOut = colors + 1;
               mapOut++;
            }
            *mapOut = top_colors_start;
            mapOut++;
            *mapOut = top_colors_end - 1;
            mapOut++;
            *mapOut = air_start;
            mapOut++;

            for (z=0; z < top_colors_len; ++z) {
               mapvxlWriteColor(&mapOut, &map->blocks[i][j][top_colors_start + z]);
            }
            for (z=0; z < bottom_colors_len; ++z) {
               mapvxlWriteColor(&mapOut, &map->blocks[i][j][bottom_colors_start + z]);
            }
         }  
      }
   }
   return mapOut - originalSize;
}

unsigned int mapvxlGetColor(MapVxl *map, int x, int y, int z) {
   Block* block = &map->blocks[x][y][z];
   if (block->data == 0) {
      return DEFAULT_COLOR;
   }
   return block->raw | 0xFF000000;
}

void mapvxlSetAir(MapVxl *map, int x, int y, int z) {
   Block* block = &map->blocks[x][y][z];
   block->raw = 0x00FFFFFF & DEFAULT_COLOR;
}

unsigned char mapvxlIsSolid(MapVxl *map, int x, int y, int z) {
   return map->blocks[x][y][z].data & 0x01;
}
