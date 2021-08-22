#include <assert.h>
#include "libmapvxl.h"

static void setBlock(MapVxl *map, int x, int y, int z, int t) {
   assert(z >= 0 && z < 64);
   map->blocks[x][y][z] = t;
}

static void setColor(MapVxl *map, int x, int y, int z, unsigned int c) {
   assert(z >= 0 && z < 64);
   map->color[x][y][z] = c;
}

void mapvxlLoadVXL(MapVxl *map, unsigned char *v) {
    int x,y,z;
    for (y=0; y < 512; ++y) {
        for (x=0; x < 512; ++x) {
            for (z=0; z < 64; ++z) {
                setBlock(map, x,y,z,1);
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
                    setBlock(map, x,y,i,0);

                color = (unsigned int *) (v+4);
                for(z=top_color_start; z <= top_color_end; z++)
                    setColor(map, x,y,z,*color++);

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
                    setColor(map, x,y,z,*color++);
                }
            }
        }
    }
}

unsigned char mapvxlIsSurface(MapVxl *map, int x, int y, int z) {
   if (map->blocks[x][y][z]==0) return 0;
   if (x   >   0 && map->blocks[x-1][y][z]==0) return 1;
   if (x+1 < 512 && map->blocks[x+1][y][z]==0) return 1;
   if (y   >   0 && map->blocks[x][y-1][z]==0) return 1;
   if (y+1 < 512 && map->blocks[x][y+1][z]==0) return 1;
   if (z   >   0 && map->blocks[x][y][z-1]==0) return 1;
   if (z+1 <  64 && map->blocks[x][y][z+1]==0) return 1;
   return 0;
}

void mapvxlWriteColor(unsigned char **mapOut, unsigned int color) {
   // assume color is ARGB native, but endianness is unknown

   // file format endianness is ARGB little endian, i.e. B,G,R,A
   (*mapOut)[0] = (color >> 0);
   (*mapOut)[1] = (color >> 8);
   (*mapOut)[2] = (color >> 16);
   (*mapOut)[3] = (color >> 24);
   *mapOut += 4;
}


void mapvxlWriteMap(MapVxl *map, unsigned char *mapOut) {
   int i,j,k;

   for (j=0; j < MAP_X_MAX; ++j) {
      for (i=0; i < MAP_Y_MAX; ++i) {
         k = 0;
         while (k < MAP_Z_MAX) {
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
            while (k < MAP_Z_MAX && !map->blocks[i][j][k])
               ++k;

            // find the top region
            top_colors_start = k;
            while (k < MAP_Z_MAX && mapvxlIsSurface(map, i,j,k))
               ++k;
            top_colors_end = k;

            // now skip past the solid voxels
            while (k < MAP_Z_MAX && map->blocks[i][j][k] && !mapvxlIsSurface(map,i,j,k))
               ++k;

            // at the end of the solid voxels, we have colored voxels.
            // in the "normal" case they're bottom colors; but it's
            // possible to have air-color-solid-color-solid-color-air,
            // which we encode as air-color-solid-0, 0-color-solid-air
          
            // so figure out if we have any bottom colors at this point
            bottom_colors_start = k;

            z = k;
            while (z < MAP_Z_MAX && mapvxlIsSurface(map, i, j, z))
               ++z;

            if (z == MAP_Z_MAX || 0)
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

            if (k == MAP_Z_MAX) {
               *mapOut = 0;
               mapOut += 1;
            }
            else{
               *mapOut = colors + 1;
               mapOut += 1;
            }
            *mapOut = top_colors_start;
            mapOut += 1;
            *mapOut = top_colors_end - 1;
            mapOut += 1;
            *mapOut = air_start;
            mapOut += 1;

            for (z=0; z < top_colors_len; ++z) {
               mapvxlWriteColor(&mapOut, map->color[i][j][top_colors_start + z]);
            }
            for (z=0; z < bottom_colors_len; ++z) {
               mapvxlWriteColor(&mapOut, map->color[i][j][bottom_colors_start + z]);
            }
         }  
      }
   }
}

unsigned int mapvxlGetColor(MapVxl *map, int x, int y, int z) {
    if (map->blocks[x][y][z] == 0) {
        return DEFAULT_COLOR;
    }
    return map->color[x][y][z];
}

void mapvxlSetAir(MapVxl *map, int x, int y, int z) {
    map->blocks[x][y][z] = 0;
    map->color[x][y][z] = 0;
}

unsigned char mapvxlIsSolid(MapVxl *map, int x, int y, int z) {
    unsigned char ret = map->blocks[x][y][z];
    if (ret > 1 || ret < 0) {
        return 2; //Error case
    }
    return ret;
}
