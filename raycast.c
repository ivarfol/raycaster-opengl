#include <stdio.h>
#include <GL/freeglut.h>
#include <math.h>
#include <stdbool.h>
#include "main.h"

#define PI 3.1415926535
#define PLAYER_SPEED 0.2f
#define RES 256
#define SCALE 4
#define HEIGHT 512
#define HSCALE 1

#define MAXSCALE ((SCALE>HSCALE)?SCALE:HSCALE)

#define PLAYER_HEIGHT (HEIGHT / 2)

#define FOV (0.5 * PI)
#define SHIFT (FOV / 2)

#define MAP_SCALE 16

#define LIGHT_GRID 2
#define LIGHT_POW 1

#define TEXLENGTH 64

#define TILE 64
#define TILE_POW 6

#define MIN_BRIGHTNESS 0.005
#define LDOF 10
#define LTEX_S (32 * (TILE / LIGHT_GRID))

#define OPACITY 0.25
#define DOOR_WAIT 300
#define MS_PER_FRAME 16

#define INVIS_R 0
#define INVIS_G 0
#define INVIS_B 0

#define MAGIC (HEIGHT * HSCALE / SCALE / TILE)

#define SQR(a) ((a)*(a))

enum { red, green, blue };

FILE* mapf;
bool firstmap;

typedef struct {
    unsigned w : 1;
    unsigned a : 1;
    unsigned s : 1;
    unsigned d : 1;
    unsigned q : 1;
    unsigned e : 1;
    unsigned m : 1;
    unsigned f : 1;
    unsigned space : 1;
    unsigned p : 1;
    unsigned R : 1;
} keys;
keys oldkeys;
keys newkeys;

typedef struct {
    float posX;
    float posY;
    float angle;
} player_struct;
player_struct player;

typedef struct {
    float dist;
    int index;
} distindex;

typedef struct {
    float x;
    float y;
} fpoint;
fpoint *vert;

typedef struct {
    int x, y, wait;
    float exte, exte_rate;
    fpoint door_vert[3];
} door_struct;
door_struct *doors;

typedef struct {
    bool is_static;
    float posX;
    float posY;
    bool state;
    float intens;
    float r;
    float g;
    float b;
} light_source;
light_source *stat_lights;
light_source *lights;

typedef struct {
    float direction;
    float deltadir;
    float deltamove;
    light_source* source;
} active_light_struct;
active_light_struct *ac_lights;

typedef struct {
    float posX;
    float posY;
    float direction;
    float deltadir;
    float deltamove;
    float (*texture)[SQR(TEXLENGTH)][CHANNELS];
} sprite_struct;
sprite_struct *sprites;

typedef struct {
    int interact;
    int subindex;
    int enterX;
    int enterY;
    int exitX;
    int exitY;
    int rotation;
} teleport_info;
teleport_info *teleport_arr;
teleport_info current_teleport;

typedef struct {
    int posX;
    int posY;
    bool sides[4];
    float (*texture)[SQR(TEXLENGTH)][CHANNELS];
    int num_of_indexes[4];
    int lights_index[16][4];
    int teleport[4];
} ornament_struct;
ornament_struct *ornaments;
bool *has_ornament;
int ornament_num;

typedef struct {
    float (*texture)[SQR(TEXLENGTH)][CHANNELS];
    float rayX;
    float rayY;
    float l_height;
    float textureX;
    float correct_fish;
    double Cos;
    double Sin;
    float dist;
    int bright_index;
    ornament_struct *ornament;
} line_out_info;
line_out_info render_info[RES];

float texture_missing[SQR(TEXLENGTH)][CHANNELS];
float texture_one[SQR(TEXLENGTH)][CHANNELS];
float texture_door[SQR(TEXLENGTH)][CHANNELS];
float texture_floor[SQR(TEXLENGTH)][CHANNELS];
float texture_ceil[SQR(TEXLENGTH)][CHANNELS];

double ltexture[SQR(LTEX_S)];

float angles[RES], fog[CHANNELS];

double floor_const;
bool show_map = true;
float current_frame = 0.0;
float delta_frames, last_frame, pla_sep, pla_sep_scaled;
float DOF, fade_start, delta_fade;
float *stationary, *brightness;

int *map, light_map_length, mapX, mapY, sprite_num, ac_light_num;
int doornum, vnum, submapn, stat_light_num, teleportnumber;
int curr_submap_index = 0;
int exit_level = -1;

long int *submapindex;

bool *visible, *expandable;
bool submap_exit = false;

void initmap(FILE*, int);

/*
cleans up the allocated memory
params
return
*/
void exitmap(void) {
    if (!submap_exit) {
        free(submapindex);
        submapindex = NULL;
    }
    switch (exit_level) {
    case 13:
        free(ornaments);
        ornaments = NULL;
    case 12:
        free(teleport_arr);
        teleport_arr = NULL;
    case 11:
        free(sprites);
        sprites = NULL;
    case 10:
        free(lights);
        lights = NULL;
    case 9:
        free(ac_lights);
        ac_lights = NULL;
    case 8:
        free(stat_lights);
        stat_lights = NULL;
    case 7:
        free(vert);
        vert = NULL;
    case 6:
        free(doors);
        doors = NULL;
    case 5:
        free(brightness);
        brightness = NULL;
    case 4:
        free(stationary);
        stationary = NULL;
    case 3:
        free(expandable);
        expandable = NULL;
    case 2:
        free(visible);
        visible = NULL;
    case 1:
        free(has_ornament);
        has_ornament = NULL;
    case 0:
        free(map);
        map = NULL;
    }
}

/*
converts from any angle to angle between 0 and 2 pi
params
*a - the angle in radians
*/
void radian_change(float *a) {
    if (*a > 2 * PI)
        *a -= 2 * PI;
    else if (*a < 0)
        *a += 2 * PI;
    if (*a > 2 * PI || *a < 0)
        radian_change(a);
}

/*
gets a door by its coordinates on the map
params
x - x position of the door
y - y position of the door
return
&doors[i] - pointer to the door at that position
*/
door_struct* getDoor(int x, int y) {
    int i;
    for (i=0;(doors[i].x != x || doors[i].y != y) && i < doornum;i++);
    if (i == doornum)
        return NULL;
    return(&doors[i]);
}

/*
gets ornament (texture on one side of a tile) by its position on the map
params
x - x position of the door
y - y position of the door
_ornaments[] - array of ornaments
return
&_ornaments[i] - pointer to the ornament at that position
*/
ornament_struct* getOrnament(int x, int y, ornament_struct _ornaments[]) {
    int i;
    for (i=0;(_ornaments[i].posX != x || _ornaments[i].posY != y) && i < ornament_num;i++);
    if (i == ornament_num)
        return NULL;
    return(&_ornaments[i]);
}

/*
checks if there is a line of sight between thwo points
params
posX - starting x
posY - starting y
angle - angle to the second point
targdist - distance to the second point
is_static - treat doors as closed if true
return
out - the point where the line of sight ends, if line dist is less than targdist out.x is set to -TILE
*/
point path_free(float posX, float posY, float angle, float targdist, bool is_static) {
    int dofH = LDOF;
    int dofV = LDOF;
    float deltaYV, deltaYH;
    float deltaXV, deltaXH;
    float rayXV, rayYV;
    float rayXH, rayYH;
    float deltadistH, deltadistV;
    float distH = 10000.0f;
    float distV = 10000.0f;

    double Cos = cos(angle);
    double Sin = sin(angle);
    double Tan = tan(angle);
    double invTan = 1 / Tan;

    //up/down
    if (Sin < -0.0001) {
        deltaYH = - TILE;
        deltaXH = - TILE * invTan;
        deltadistH = - TILE / Sin;
        rayYH = ((((int)posY)>>TILE_POW)<<TILE_POW) - 0.0001;
        rayXH = posX - (posY- rayYH) * invTan;
        distH = - (posY- rayYH) / Sin;
        dofH = 0;
    }
    else if (Sin > 0.0001) {
        deltaYH = TILE;
        deltaXH = TILE * invTan;
        deltadistH = TILE / Sin;
        rayYH = ((((int)posY)>>TILE_POW)<<TILE_POW) + TILE;
        rayXH = posX - (posY- rayYH) * invTan;
        distH = - (posY- rayYH) / Sin;
        dofH = 0;
    }
    else {
        rayXH = posX ;
        rayYH = posY;
        distH = 10000.0f;
        dofH = LDOF;
    }

    //left/right
    if (Cos > 0.0001) {
        deltaXV = TILE;
        deltaYV = TILE * Tan;
        deltadistV = TILE / Cos;
        rayXV = ((((int)posX )>>TILE_POW)<<TILE_POW) + TILE;
        rayYV = posY- (posX - rayXV) * Tan;
        distV = - (posX - rayXV) / Cos;
        dofV = 0;
    }
    else if (Cos < -0.0001) {
        deltaXV = - TILE;
        deltaYV = - TILE * Tan;
        deltadistV = - TILE / Cos;
        rayXV = ((((int)posX )>>TILE_POW)<<TILE_POW) - 0.001;
        rayYV = posY- (posX - rayXV) * Tan;
        distV = - (posX - rayXV) / Cos;
        dofV = 0;
    }
    else {
        rayXV = posX;
        rayYV = posY;
        distV = 10000.0f;
        dofV = LDOF;
    }
    
    float deltadist_mult = TILE / LIGHT_GRID;
    int raymapXH, raymapYH, raymapH;
    int raymapXV, raymapYV, raymapV;
    bool Hpropagate, Vpropagate;
    door_struct *doorH, *doorV;
    while (dofV < LDOF || dofH < LDOF) {
        if (dofH < DOF && distH <= distV) {
            Hpropagate = false;
            raymapXH = (int)(rayXH)>>TILE_POW;
            raymapYH = (int)(rayYH)>>TILE_POW;
            raymapH = raymapXH + raymapYH * mapX;
            if (raymapH >= 0 && raymapH < mapX * mapY) {
                if (map[raymapH] == 1) {
                    dofH = DOF;
                    doorH = NULL;
                    break;
                }
                else if (map[raymapH] == 12 && (int)(rayXH + 0.5 * deltaXH)>>TILE_POW == (int)(rayXH)>>TILE_POW) {
                    doorH = getDoor(raymapXH, raymapYH);
                    if (doorH->x * TILE - ceil(doorH->exte) + TILE < rayXH + 0.5 * deltaXH || is_static) {
                        dofH = DOF;
                        distH += 0.5 * deltadistH;
                        rayXH += 0.5 * deltaXH;
                        rayYH += 0.5 * deltaYH;
                        break;
                    }
                    else
                        Hpropagate = true;
                }
                else if (map[raymapH] == 13 && (int)(rayXH + 0.5 * deltaXH)>>TILE_POW == (int)(rayXH)>>TILE_POW) {
                    dofH = DOF;
                    distH += 0.5 * deltadistH;
                    rayXH += 0.5 * deltaXH;
                    rayYH += 0.5 * deltaYH;
                    doorH = NULL;
                    doorV = NULL;
                    break;
                }
                else
                    Hpropagate = true;
            }
            else
                Hpropagate = true;
            if (Hpropagate) {
                rayXH += deltaXH;
                rayYH += deltaYH;
                distH += deltadistH;
                dofH++;
            }
        }
        else if (dofV < DOF && distV < distH) {
            Vpropagate = false;
            raymapXV = (int)(rayXV)>>TILE_POW;
            raymapYV = (int)(rayYV)>>TILE_POW;
            raymapV = raymapXV + raymapYV * mapX;
            if (raymapV >= 0 && raymapV < mapX * mapY) {
                if (map[raymapV] == 1) {
                    dofV = DOF;
                    doorV = NULL;
                    break;
                }
                else if (map[raymapV] == 2 && (int)(rayYV + 0.5 * deltaYV)>>TILE_POW == (int)(rayYV)>>TILE_POW) {
                    doorV = getDoor(raymapXV, raymapYV);
                    if (doorV->y * TILE - ceil(doorV->exte) + TILE < rayYV + 0.5 * deltaYV || is_static) {
                        dofV = DOF;
                        distV += 0.5 * deltadistV;
                        rayXV += 0.5 * deltaXV;
                        rayYV += 0.5 * deltaYV;
                        break;
                    }
                    else
                        Vpropagate = true;
                }
                else if (map[raymapV] == 3 && (int)(rayYV + 0.5 * deltaYV)>>TILE_POW == (int)(rayYV)>>TILE_POW) {
                    dofV = DOF;
                    distV += 0.5 * deltadistV;
                    rayXV += 0.5 * deltaXV;
                    rayYV += 0.5 * deltaYV;
                    doorH = NULL;
                    doorV = NULL;
                    break;
                }
                else
                    Vpropagate = true;
            }
            else
                Vpropagate = true;
            if (Vpropagate) {
                rayXV += deltaXV;
                rayYV += deltaYV;
                distV += deltadistV;
                dofV++;
            }
        }
        else
            break;
    }
    point out;
    if (distV < distH) {
        out.x = (rayXV)  / LIGHT_GRID;
        out.y = (rayYV)  / LIGHT_GRID;

        if (raymapV > mapX * mapY || raymapV < 0)
            out.x = -TILE;
        else {
        if (map[raymapV] == 12 || map[raymapV] == 13) {
            if (deltaYV > 0)
                out.y -= 1;
            else
                out.y += 1;
        }
        else if (map[raymapV] == 2 || map[raymapV] == 3) {
            if (deltaXV > 0)
                out.x -= 1;
            else
                out.x += 1;
            if (deltaYV < 0)
                out.y -= 1;
        }

        if (deltaYV > 0)
            out.y += 1;

        if (distV + TILE / LIGHT_GRID < targdist || out.y < 0 || out.y > mapY * TILE / LIGHT_GRID || out.x < 0 || out.x > mapX * TILE / LIGHT_GRID)
            out.x = -TILE;
        }
    }
    else {
        out.x = (rayXH)  / LIGHT_GRID;
        out.y = (rayYH)  / LIGHT_GRID;
        if (raymapH > mapX * mapY || raymapH < 0)
            out.x = -TILE;
	else {
        if (map[raymapH] == 12 || map[raymapH] == 13) {
            if (deltaYH > 0)
                out.y -= 1;
            else
                out.y += 1;
        }
        else if (map[raymapH] == 2 || map[raymapH] == 3) {
            if (deltaXH > 0)
                out.x -= 1;
            else
                out.x += 1;
        }

        if (deltaYH > 0)
            out.y += 1;
        if (deltaXH > 0)
            out.x += 1;
        else
            out.x -= 1;

        if (distH + TILE / LIGHT_GRID < targdist || out.y < 0 || out.y > mapY * TILE / LIGHT_GRID || out.x < 0 || out.x > mapX * TILE / LIGHT_GRID)
            out.x = -TILE;
        }
    }
    return out;
}

/*
compare function for quick sort
params
*a - number to compare
*b - number to compare
return
*(int*)a - *(int*)b - difference
*/
int compare(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

/*
generates general light map to use in other calculations
params
side - length of one side
l_texture[] - texture to write the lightmap to
return
*/
void genltex(int side, double l_texture[]) {
    int i, j, texindex;
    double tmp;
    for (i=0;i<side/2;i++) {
        for (j=0;j<side/2;j++) {
            tmp = 1.0 / (double)(SQR(side / 2.0 * LIGHT_GRID - i * LIGHT_GRID) + SQR(side / 2.0 * LIGHT_GRID - j * LIGHT_GRID));
            if (tmp > 1.0)
                tmp = 1.0;
            texindex = i * side + j;
            l_texture[texindex] = tmp;
            texindex = side + i * side - j - 1;
            l_texture[texindex] = tmp;
            texindex = SQR(side) - side - i * side + j;
            l_texture[texindex] = tmp;
            texindex = SQR(side) - i * side - j - 1;
            l_texture[texindex] = tmp;
        }
    }
}

/*
generates missing textures
params
texture[][CHANNELS] - the texture to write it to
return
*/
void genmissing(float texture[][CHANNELS]) {
    int i, j, tmpc;
    for (i=0;i<TEXLENGTH ;i++) {
        for (j=0;j<TEXLENGTH ;j++) {
            if ((i < TEXLENGTH / 2 && j < TEXLENGTH / 2) || (i >= TEXLENGTH / 2&& j >= TEXLENGTH / 2))
                tmpc = 1;
            else
                tmpc = 0;
            texture[i * TEXLENGTH + j][red] = tmpc;
            texture[i * TEXLENGTH + j][green] = 0;
            texture[i * TEXLENGTH + j][blue] = tmpc;
        }
    }
}

/*
generates light map for a light source
params
light - the light source
light_map[] - array to write the light map to
return
*/
void gen_light(light_source light, float light_map[]) {
    int startX, endX;
    int startY, endY;
    float max_dist = sqrt(light.intens / MIN_BRIGHTNESS);
    startX = (int)(light.posX- max_dist)>>LIGHT_POW;
    if (startX < 0)
        startX = 0;
    endX = (int)(light.posX+ max_dist)>>LIGHT_POW;
    if (endX > mapX * (TILE / LIGHT_GRID))
        endX = mapX * (TILE / LIGHT_GRID);

    startY = (int)(light.posY- max_dist)>>LIGHT_POW;
    if (startY < 0)
        startY = 0;
    endY = (int)(light.posY+ max_dist)>>LIGHT_POW;
    if (endY > mapY * (TILE / LIGHT_GRID))
        endY = mapY * (TILE / LIGHT_GRID);

    int tileY, tileX;
    float bright;
    int i, color;
    bool shadow[light_map_length];
    for (i=0;i<light_map_length;i++) {
        shadow[i] = 0;
        for (color=0;color<CHANNELS;color++) {
            light_map[i * CHANNELS + color] = 0;
        }
    }

    vert[vnum-1].x = startX<<LIGHT_POW;
    vert[vnum-1].y = endY<<LIGHT_POW;

    vert[vnum-2].x = endX<<LIGHT_POW;
    vert[vnum-2].y = startY<<LIGHT_POW;;

    vert[vnum-3].x = endX<<LIGHT_POW;
    vert[vnum-3].y = endY<<LIGHT_POW;

    vert[vnum-4].x = startX<<LIGHT_POW;
    vert[vnum-4].y = startY<<LIGHT_POW;

    int vnumaf = 0;
    bool isactive[vnum];
    for (i=0;i<vnum;i++) {
        if (((int)vert[i].x>>LIGHT_POW) > startX - TILE / LIGHT_GRID && ((int)vert[i].y>>LIGHT_POW) > startY - TILE / LIGHT_GRID && ((int)vert[i].x>>LIGHT_POW) < endX + TILE / LIGHT_GRID && ((int)vert[i].y>>LIGHT_POW) < endY + TILE / LIGHT_GRID) {
            isactive[i] = true;
            vnumaf++;
        }
        else
            isactive[i] = false;
    }
    int door_vnumaf = 0;
    bool door_vert_active[doornum][3];
    int j;
    for (i=0;i<doornum;i++) {
        if (((int)doors[i].door_vert[0].x>>LIGHT_POW) > startX - TILE / LIGHT_GRID && ((int)doors[i].door_vert[0].y>>LIGHT_POW) > startY - TILE / LIGHT_GRID && ((int)doors[i].door_vert[0].x>>LIGHT_POW) < endX + TILE / LIGHT_GRID && ((int)doors[i].door_vert[0].y>>LIGHT_POW) < endY + TILE / LIGHT_GRID) {
            if (doors[i].exte != 0 || light.is_static) {
                door_vert_active[i][0] = true;
                door_vert_active[i][1] = true;
                door_vnumaf += 2;
            }
            else {
                door_vert_active[i][0] = false;
                door_vert_active[i][1] = false;
            }
            if (doors[i].exte_rate != 0.0 && doors[i].wait == 0 && !light.is_static)  {
                door_vert_active[i][2] = true;
                door_vnumaf++;
            }
            else
                door_vert_active[i][2] = false;
        }
        else {
            door_vert_active[i][0] = false;
            door_vert_active[i][1] = false;
            door_vert_active[i][2] = false;
        }
    }

    float vangle[vnumaf+door_vnumaf][2];
    vnumaf = 0;
    for (i=0;i<vnum;i++) {
        if (isactive[i]) {
            vangle[vnumaf][1] = sqrt(SQR(vert[i].x - light.posX) + SQR(vert[i].y - light.posY));
            vangle[vnumaf][0] = acos((vert[i].x - light.posX)/vangle[vnumaf][1]);
            if (vert[i].y - light.posY < 0) {
                vangle[vnumaf][0] = 2*PI - vangle[vnumaf][0];
            }
            radian_change(&vangle[vnumaf][0]);
            vnumaf++;
        }
    }
    for (i=0;i<doornum;i++) {
        for (j=0;j<3;j++) {
            if (door_vert_active[i][j]) {
                if (light.is_static && j == 1) {
                    if (map[doors[i].y * mapX + doors[i].x] == 2) {
                        vangle[vnumaf][1] = sqrt(SQR(doors[i].door_vert[j].x - light.posX) + SQR(doors[i].y * TILE + 0.001 - light.posY));
                        vangle[vnumaf][0] = acos((doors[i].door_vert[j].x - light.posX)/vangle[vnumaf][1]);
                    }
                    else {
                        vangle[vnumaf][1] = sqrt(SQR(doors[i].x * TILE + 0.001 - light.posX) + SQR(doors[i].door_vert[j].y - light.posY));
                        vangle[vnumaf][0] = acos((doors[i].x * TILE + 0.001 - light.posX)/vangle[vnumaf][1]);
                    }
                }
                else {
                    vangle[vnumaf][1] = sqrt(SQR(doors[i].door_vert[j].x - light.posX) + SQR(doors[i].door_vert[j].y - light.posY));
                    vangle[vnumaf][0] = acos((doors[i].door_vert[j].x - light.posX)/vangle[vnumaf][1]);
                }
                if (doors[i].door_vert[j].y - light.posY < 0) {
                    vangle[vnumaf][0] = 2*PI - vangle[vnumaf][0];
                }
                radian_change(&vangle[vnumaf][0]);
                vnumaf++;
            }
        }
    }
    qsort(vangle, vnumaf, sizeof(vangle[0]), compare);
    point interv[3];
    interv[0].x = (int)(light.posX) >> LIGHT_POW;
    interv[0].y = (int)(light.posY) >> LIGHT_POW;

    i = 0;

    interv[1] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1], light.is_static);
    while (interv[1].x == -TILE) {
        i++;
        if (i >= vnumaf)
            break;
        interv[1] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1], light.is_static);
    }
    point startp;
    startp = interv[1];

    bool lastisone = false;
    for (;i<vnumaf;i++) {
        interv[2] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1], light.is_static);
        while (interv[2].x == -TILE || (interv[2].x == interv[1].x && interv[2].y == interv[1].y)) {
            i++;
            if (i >= vnumaf)
                break;
            interv[2] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1], light.is_static);
        }
        if (i >= vnumaf)
            break;
        lastisone = true;
        filled_tr(interv, shadow, mapX*(TILE/LIGHT_GRID));
        do {
            i++;
            if (i >= vnumaf)
                break;
            interv[1] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1], light.is_static);
        } while (interv[1].x == -TILE || (interv[2].x == interv[1].x && interv[2].y == interv[1].y));
        if (i >= vnumaf)
            break;
        filled_tr(interv, shadow, mapX*(TILE/LIGHT_GRID));
        lastisone = false;
    }
    if (lastisone)
        interv[1] = startp;
    else
        interv[2] = startp;
    if (!(interv[2].x == interv[1].x && interv[2].y == interv[1].y))
        filled_tr(interv, shadow, mapX*(TILE/LIGHT_GRID));

    float mult;
    int bright_index;
    for (tileY = startY;tileY<endY;tileY++) {
        for (tileX = startX;tileX<endX;tileX++) {
            if (visible[(tileY>>(TILE_POW-LIGHT_POW))*mapX + (tileX>>(TILE_POW-LIGHT_POW))] || light.is_static) {
                bright_index = tileY * mapX * (TILE / LIGHT_GRID) + tileX;
                mult = OPACITY;
                if (shadow[bright_index])
                    mult = 1;
                bright = light.intens * mult * ltexture[(LTEX_S / 2 - ((int)(light.posY)>>LIGHT_POW) + tileY) * LTEX_S + LTEX_S / 2 - ((int)(light.posX)>>LIGHT_POW) + tileX];
                if (bright > 1)
                    bright = 1;
                else if (bright < 0)
                    bright = 0;
                light_map[bright_index * CHANNELS + red] = bright * light.r;
                light_map[bright_index * CHANNELS + green] = bright * light.g;
                light_map[bright_index * CHANNELS + blue] = bright * light.b;
            }
        }
    }
}

/*
adds two light maps
params
light_one[] - light map to add to
light_two[] - light map to add
return
*/
void combine_light(float light_one[], float light_two[]) {
    int i, color;
    for (i=0;i<light_map_length;i++) {
        for (color=0;color<CHANNELS;color++) {
            light_one[i * CHANNELS + color] += light_two[i * CHANNELS + color];
        }
    }
}

/*
subtracts light maps
params
light_one[] - light map to subtract from
light_two[] - light map to subtract
*/
void subtract_light(float light_one[], float light_two[]) {
    int i, color;
    for (i=0;i<light_map_length;i++) {
        for (color=0;color<CHANNELS;color++) {
            light_one[i * CHANNELS + color] -= light_two[i * CHANNELS + color];
        }
    }
}

/*
draws the minimap
params
return
*/
void draw_map() {
    int x, y, tileX, tileY;
    for (y=0;y<mapY;y++) {
        for (x=0;x<mapX;x++) {
            if (map[y*mapX+ x] == 1) {
                glColor3f(1, 1, 1);
                tileX = x * MAP_SCALE;
                tileY = y * MAP_SCALE;
                glBegin(GL_QUADS);
                glVertex2i(tileX + 1, tileY + 1);
                glVertex2i(tileX + 1, tileY + MAP_SCALE- 1);
                glVertex2i(tileX + MAP_SCALE- 1, tileY + MAP_SCALE- 1);
                glVertex2i(tileX + MAP_SCALE- 1, tileY + 1);
                glEnd();
            }
            else if (map[y*mapX + x] == 2 || map[y*mapX + x] == 3) {
                if (map[y*mapX + x] == 3)
                    glColor3f(1, 1, 1);
                else
                    glColor3f(1, 0, 0);
                tileX = (x + 0.5) * MAP_SCALE;
                tileY = y * MAP_SCALE;
                glBegin(GL_LINES);
                glVertex2i(tileX, tileY + 1);
                glVertex2i(tileX, tileY + MAP_SCALE - 1);
                glEnd();
            }
            else if (map[y*mapX + x] == 12 || map[y*mapX + x] == 13) {
                if (map[y*mapX + x] == 13)
                    glColor3f(1, 1, 1);
                else
                    glColor3f(1, 0, 0);
                tileX = x * MAP_SCALE;
                tileY = (y + 0.5) * MAP_SCALE;
                glBegin(GL_LINES);
                glVertex2i(tileX + 1, tileY);
                glVertex2i(tileX + MAP_SCALE - 1, tileY);
                glEnd();
            }
        }
    }
}

/*
moves the player/sprites/lights
params
move_direction - angle of movement
*positionX - starting x
*positionY - starting Y
speed - speed of the movement
return
*/
void movef(float move_direction, float *positionX, float *positionY, float speed) {
    door_struct* door;
    door = NULL;
    float tmpX = *positionX + cos(move_direction) * speed * delta_frames;
    float tmpY = *positionY + sin(move_direction) * speed * delta_frames;
    int tmpYmap = map[((int)tmpY>>TILE_POW) * mapX + ((int)(*positionX)>>TILE_POW)];
    int tmpXmap = map[((int)(*positionY)>>TILE_POW) * mapX + ((int)tmpX>>TILE_POW)];
    if (tmpYmap == 12 || tmpYmap == 2)
        door = getDoor(((int)(*positionX)>>TILE_POW), ((int)tmpY>>TILE_POW));
    if (tmpYmap == 0 || door != NULL && door->exte == 0.0)
        *positionY = tmpY;
    door = NULL;
    if (tmpXmap == 12 || tmpXmap == 2)
        door = getDoor(((int)tmpX>>TILE_POW), ((int)(*positionY)>>TILE_POW));
    if (tmpXmap == 0 || door != NULL && door->exte == 0.0)
        *positionX = tmpX;
}

/*
checks inputs, moves and interacts with the envierment
params
return
*/
void check_inputs() {
    float move_direction_v, move_direction_h;
    if (newkeys.p) {
        exit(0);
    }
    move_direction_h = move_direction_v = -1.0f;
    if (newkeys.a && !newkeys.d) {
        move_direction_h = 1.5 * PI;
    }
    if (newkeys.d && !newkeys.a) {
        move_direction_h = 0.5 * PI;
    }
    if (newkeys.w && !newkeys.s) {
        move_direction_v = 0.0;
    }
    if (newkeys.s && !newkeys.w) {
        move_direction_v = PI;
    }
    if (newkeys.q) {
        player.angle -= 0.005 * delta_frames;
        radian_change(&player.angle);
    }
    if (newkeys.e) {
        player.angle += 0.005 * delta_frames;
        radian_change(&player.angle);
    }
    if (newkeys.m && !oldkeys.m)
        show_map -= 1;
    if (newkeys.R && !oldkeys.R)
        genmarble(texture_ceil);
    
    if (move_direction_h >= 0.0) {
        if (move_direction_v >= 0.0) {
            move_direction_h = (move_direction_v + move_direction_h) / 2;
            if (move_direction_v == 0.0 && move_direction_h == (float)(0.75 * PI))
                move_direction_h = 1.75 * PI;
        }
        move_direction_h += player.angle;
        radian_change(&move_direction_h);
        movef(move_direction_h, &player.posX, &player.posY, PLAYER_SPEED);
    }
    else if (move_direction_v >= 0.0) {
        move_direction_v += player.angle;
        radian_change(&move_direction_v);
        movef(move_direction_v, &player.posX, &player.posY, PLAYER_SPEED);
    }

    int i;
    for (i=0;i<teleportnumber;i++) {
        if (((int)(player.posY)>>TILE_POW) == teleport_arr[i].enterY && ((int)(player.posX)>>TILE_POW) == teleport_arr[i].enterX) {
            current_teleport = teleport_arr[i];
            if (current_teleport.subindex == -1)
                exit(0);
            submap_exit = true;
            exitmap();
            initmap(mapf, current_teleport.subindex);
        }
    }

    

    float rayXH, rayYH, distH, rayXV, rayYV, distV;
    float Sin = sin(player.angle);
    float Cos = cos(player.angle);
    float Tan = Sin / Cos;
    float invTan = Cos / Sin;
    int raymapX, raymapY;
    door_struct* door;
    if (newkeys.space) {
        //up/down
        if (Sin < -0.0001) {
            rayYH = ((((int)player.posY)>>TILE_POW)<<TILE_POW) - 0.0001;
            rayXH = player.posX - (player.posY - rayYH) * invTan;
            distH = - (player.posY - rayYH) / Sin;
        }
        else if (Sin > 0.0001) {
            rayYH = ((((int)player.posY)>>TILE_POW)<<TILE_POW) + TILE;
            rayXH = player.posX - (player.posY - rayYH) * invTan;
            distH = - (player.posY - rayYH) / Sin;
        }
        else {
            rayXH = player.posX;
            rayYH = player.posY;
            distH = 10000.0f;
        }

        //left/right
        if (Cos > 0.0001) {
            rayXV = ((((int)player.posX)>>TILE_POW)<<TILE_POW) + TILE;
            rayYV = player.posY - (player.posX - rayXV) * Tan;
            distV = - (player.posX - rayXV) / Cos;
        }
        else if (Cos < -0.0001) {
            rayXV = ((((int)player.posX)>>TILE_POW)<<TILE_POW) - 0.001;
            rayYV = player.posY - (player.posX - rayXV) * Tan;
            distV = - (player.posX - rayXV) / Cos;
        }
        else {
            rayXV = player.posX;
            rayYV = player.posY;
            distV = 10000.0f;
        }
        ornament_struct *ornament;
        int side = 0;
        float st_light_map[light_map_length * CHANNELS];
        if (distV < distH) {
            if (!oldkeys.space && has_ornament[((int)(rayYV)>>TILE_POW) * mapX + ((int)(rayXV)>>TILE_POW)]) {
                ornament = getOrnament((int)(rayXV)>>TILE_POW, (int)(rayYV)>>TILE_POW, ornaments);
                if ((ornament->sides[1] && ((int)rayXV) % TILE == 63 || ornament->sides[3] && ((int)rayXV) % TILE == 0)) {
                    if (Cos > 0)
                        side = 3;
                    else if (Cos < 0)
                        side = 1;
                    if (ornament->teleport[side] != -1) {
                        current_teleport = teleport_arr[ornament->teleport[side]];
                        if (current_teleport.subindex == -1)
                            exit(0);
                        submap_exit = true;
                        exitmap();
                        initmap(mapf, current_teleport.subindex);
                    }
                    else {
                        for (i=0;i<ornament->num_of_indexes[side];i++) {
                            if (ornament->lights_index[i][side] < stat_light_num) {
                                gen_light(stat_lights[ornament->lights_index[i][side]], st_light_map);
                                if (stat_lights[ornament->lights_index[i][side]].state)
                                    subtract_light(stationary, st_light_map);
                                else
                                    combine_light(stationary, st_light_map);
                                stat_lights[ornament->lights_index[i][side]].state--;
                            }
                            else
                                ac_lights[ornament->lights_index[i][side] - stat_light_num].source->state--;
                        }
                    }
                }
            }
            rayXH = rayXV;
            rayYH = rayYV;
        }
        else {
            if (!oldkeys.space && has_ornament[((int)(rayYH)>>TILE_POW) * mapX + ((int)(rayXH)>>TILE_POW)]) {
                ornament = getOrnament((int)(rayXH)>>TILE_POW, (int)(rayYH)>>TILE_POW, ornaments);
                if ((ornament->sides[0] && ((int)rayYH) % TILE == 0 || ornament->sides[2] && ((int)rayYH) % TILE == 63)) {
                    if (Sin > 0)
                        side = 0;
                    else if (Sin < 0)
                        side = 2;
                    if (ornament->teleport[side] != -1) {
                        current_teleport = teleport_arr[ornament->teleport[side]];
                        if (current_teleport.subindex == -1)
                            exit(0);
                        submap_exit = true;
                        exitmap();
                        initmap(mapf, current_teleport.subindex);
                    }
                    else {
                        for (i=0;i<ornament->num_of_indexes[side];i++) {
                            if (ornament->lights_index[i][side] < stat_light_num) {
                                gen_light(stat_lights[ornament->lights_index[i][side]], st_light_map);
                                if (stat_lights[ornament->lights_index[i][side]].state)
                                    subtract_light(stationary, st_light_map);
                                else
                                    combine_light(stationary, st_light_map);
                                stat_lights[ornament->lights_index[i][side]].state--;
                            }
                            else
                                ac_lights[ornament->lights_index[i][side] - stat_light_num].source->state--;
                        }
                    }
                }
            }
        }
        raymapX = (int)(rayXH)>>TILE_POW;
        raymapY = (int)(rayYH)>>TILE_POW;
        if (map[raymapX + raymapY * mapX] == 2 || map[raymapX + raymapY * mapX] == 12) {
            door = getDoor(raymapX, raymapY);
            door->exte_rate = -0.5;
        }
        for (i=0;i<teleportnumber;i++) {
            if (teleport_arr[i].interact && raymapY == teleport_arr[i].enterY && raymapX == teleport_arr[i].enterX) {
                current_teleport = teleport_arr[i];
                if (current_teleport.subindex == -1)
                    exit(0);
                submap_exit = true;
                exitmap();
                initmap(mapf, current_teleport.subindex);
            }
        }
    }
    glutPostRedisplay();
}


/*
compare function for quick sort
params
*a - distance to compare
*b - distance to compare
return
distindexA->dist - distindexB->dist - difference
*/
int compare_dist(const void *a, const void *b) {
    distindex *distindexA = (distindex *)a;
    distindex *distindexB = (distindex *)b;
    return (distindexA->dist - distindexB->dist);
}

/*
renders the scene
params
return
*/
void render() {
    int ray, start, end, hposition, tex_index;
    int bright_index, lightX, lightY, i, j;
    float floor_ray, textureY, deltatextureY;
    float offset, fogstrength, r, g, b;
    distindex spr_dist[sprite_num];
    line_out_info* render_infoP = &render_info[0];
    for (i=0;i<sprite_num;i++) {
        spr_dist[i].dist = sqrt(SQR(player.posX - sprites[i].posX) + SQR(player.posY - sprites[i].posY));
        spr_dist[i].index = i;
    }
    qsort(spr_dist, sprite_num, sizeof(distindex), compare_dist);
    float spr_angle, spr_hpos, deltaLX, deltaLY, startLX, startLY;
    float delta_angle, delta_textureX, height, width, textureX;
    int columnX, br_index;
    bool mask[RES][HEIGHT];
    for (i=0;i<RES;i++) {
        for (j=0;j<HEIGHT;j++)
            mask[i][j] = true;
    }

    glPointSize(MAXSCALE);
    for (i=0;i<sprite_num && spr_dist[i].dist < (fade_start + delta_fade) * TILE;i++) {
        spr_angle = atan((player.posY - sprites[spr_dist[i].index].posY) / (player.posX - sprites[spr_dist[i].index].posX));
        if (player.posX < sprites[spr_dist[i].index].posX)
            spr_angle -= PI;
        radian_change(&spr_angle);
        delta_angle = spr_angle - player.angle;
        radian_change(&delta_angle);
        height = (float)(pla_sep_scaled * HEIGHT) / spr_dist[i].dist / -cos(delta_angle);
        width = height * HSCALE / SCALE;
        spr_hpos = pla_sep * tan(delta_angle) + RES / 2 - width / 2;
        delta_angle += 0.5 * PI;
        if (delta_angle > PI && spr_hpos < RES && spr_hpos + width > 0) {
            deltatextureY = TEXLENGTH / height;
            offset = 0.0f;
            if (height > HEIGHT) {
                start = 0;
                end = HEIGHT;
                offset = (height - HEIGHT) / 2.0;
            }
            else {
                start = HEIGHT / 2.0 - 0.5 * height;
                end = HEIGHT / 2.0 + 0.5 * height;
            }
            offset *= deltatextureY;
            delta_textureX = TEXLENGTH / width;
            textureX = 0;

            fogstrength = -fade_start / delta_fade + spr_dist[i].dist / (float)(TILE) / delta_fade;
            if (fogstrength < 0)
                fogstrength = 0;
            else if (fogstrength > 1.0)
                fogstrength = 1.0;

            startLX = sprites[spr_dist[i].index].posX;
            startLY = sprites[spr_dist[i].index].posY;
            deltaLX = TILE / width * -sin(player.angle);
            deltaLY = TILE / width * cos(player.angle);
            startLX -= width * deltaLX / 2;
            startLY -= width * deltaLY / 2;
            for (columnX = spr_hpos;columnX < RES && columnX < spr_hpos + width-1;columnX++) {
                if (columnX > 0 && sqrt(SQR(player.posX - startLX) + SQR(player.posY - startLY)) < render_info[columnX].dist) {
                    br_index = ((int)(startLY)>>LIGHT_POW) * mapX * (TILE / LIGHT_GRID) + ((int)(startLX)>>LIGHT_POW);
                    textureY = offset;
                    for (hposition = start;hposition<end;hposition++) {
                        if (mask[columnX][hposition]) {
                            tex_index= (int)((int)(textureY) * TEXLENGTH + textureX);
                            r = (*sprites[spr_dist[i].index].texture)[tex_index][red];
                            g = (*sprites[spr_dist[i].index].texture)[tex_index][green];
                            b = (*sprites[spr_dist[i].index].texture)[tex_index][blue];
                            if (!(r == INVIS_R && g == INVIS_G && b == INVIS_B)) {
                                r *= brightness[br_index * CHANNELS + red];
                                g *= brightness[br_index * CHANNELS + green];
                                b *= brightness[br_index * CHANNELS + blue];
                                r = r * (1.0 - fogstrength) + fog[red] * fogstrength;
                                g = g * (1.0 - fogstrength) + fog[green] * fogstrength;
                                b = b * (1.0 - fogstrength) + fog[blue] * fogstrength;

                                glColor3f(r, g, b);
                                glBegin(GL_POINTS);
                                glVertex2i(columnX * SCALE - SCALE, hposition * HSCALE);
                                glEnd();
                                mask[columnX][hposition] = false;
                            }
                            textureY += deltatextureY;
                        }
                    }
                }

                textureX += delta_textureX;
                startLX += deltaLX;
                startLY += deltaLY;
            }
        }
    }
    for (ray=0;ray<RES;ray++) {
        if (show_map) {
            glColor3f(0, 1, 0);
            glLineWidth(1);
            glBegin(GL_LINES);
            glVertex2i(player.posX / TILE * MAP_SCALE, player.posY / TILE * MAP_SCALE);
            glVertex2i(render_infoP->rayX / TILE * MAP_SCALE, render_infoP->rayY / TILE * MAP_SCALE);
            glEnd();
        }

        deltatextureY = TEXLENGTH / render_infoP->l_height;
        offset = 0.0f;
        if (render_infoP->l_height > HEIGHT) {
            start = 0;
            end = HEIGHT;
            offset = (render_infoP->l_height - HEIGHT) / 2.0;
        }
        else {
            start = HEIGHT / 2.0 - 0.5 * render_infoP->l_height;
            end = HEIGHT / 2.0 + 0.5 * render_infoP->l_height;
        }
        textureY = offset * deltatextureY;

        for (hposition=start; hposition<end;hposition++) {
            if (mask[ray][hposition]) {
                tex_index= (int)((int)(textureY) * TEXLENGTH + render_infoP->textureX);
                bool skipornament = false;

                if (render_infoP->ornament != NULL) {
                    r = (*render_infoP->ornament->texture)[tex_index][red];
                    g = (*render_infoP->ornament->texture)[tex_index][green];
                    b = (*render_infoP->ornament->texture)[tex_index][blue];
                    if (r == 0.0 && g == 0.0 && b == 0.0)
                        skipornament = true;
                }
                if (render_infoP->ornament == NULL || skipornament) {
                    r = (*render_infoP->texture)[tex_index][red];
                    g = (*render_infoP->texture)[tex_index][green];
                    b = (*render_infoP->texture)[tex_index][blue];
                }

                fogstrength = -fade_start / delta_fade + render_infoP->dist / (float)(TILE) / delta_fade;
                if (fogstrength < 0)
                    fogstrength = 0;
                else if (fogstrength > 1.0)
                    fogstrength = 1.0;
                
                r *= brightness[render_infoP->bright_index * CHANNELS + red];
                g *= brightness[render_infoP->bright_index * CHANNELS + green];
                b *= brightness[render_infoP->bright_index * CHANNELS + blue];
                r = r * (1.0 - fogstrength) + fog[red] * fogstrength;
                g = g * (1.0 - fogstrength) + fog[green] * fogstrength;
                b = b * (1.0 - fogstrength) + fog[blue] * fogstrength;

                glColor3f(r, g, b);
                glBegin(GL_POINTS);
                glVertex2i(ray * SCALE - SCALE, hposition * HSCALE);
                glEnd();
            }
            textureY += deltatextureY;
        }
        for (hposition = 0;hposition<HEIGHT - end;hposition++) {
            if (mask[ray][hposition+end] || mask[ray][start - hposition]) {
                floor_ray = floor_const / (hposition + end - HEIGHT / 2.0) / render_infoP->correct_fish;
                fogstrength = -fade_start / delta_fade + floor_ray / (float)(TILE) / delta_fade;
                if (fogstrength < 0)
                    fogstrength = 0;
                else if (fogstrength > 1.0)
                    fogstrength = 1.0;
                tex_index = (int)(floor_ray * render_infoP->Sin + player.posY) % TEXLENGTH * TEXLENGTH + (int)(floor_ray * render_infoP->Cos + player.posX) % TEXLENGTH ;
                if (tex_index >= TEXLENGTH * TEXLENGTH )
                    tex_index = TEXLENGTH * TEXLENGTH -1;
                r = texture_floor[tex_index][red];
                g = texture_floor[tex_index][green];
                b = texture_floor[tex_index][blue];
                
                lightX = (int)(floor_ray * render_infoP->Cos + player.posX)>>LIGHT_POW;
                lightY = (int)(floor_ray * render_infoP->Sin + player.posY)>>LIGHT_POW;
                bright_index = lightY*mapX*(TILE/LIGHT_GRID) + lightX;
                if (light_map_length * CHANNELS <= bright_index || bright_index < 0)
                    bright_index = 0;

                if (mask[ray][hposition+end]) {
                    r *= brightness[bright_index * CHANNELS + red];
                    g *= brightness[bright_index * CHANNELS + green];
                    b *= brightness[bright_index * CHANNELS + blue];
                    r = r * (1.0 - fogstrength) + fog[red] * fogstrength;
                    g = g * (1.0 - fogstrength) + fog[green] * fogstrength;
                    b = b * (1.0 - fogstrength) + fog[blue] * fogstrength;
                    glColor3f(r, g, b);
                    glBegin(GL_POINTS);
                    glVertex2i(ray * SCALE-SCALE, hposition * HSCALE + end * HSCALE);
                    glEnd();
                }
                if (mask[ray][start - hposition]) {
                    //r = g = b = 1.0;
                    r = texture_ceil[tex_index][red];
                    g = texture_ceil[tex_index][green];
                    b = texture_ceil[tex_index][blue];
                    
                    r *= brightness[bright_index * CHANNELS + red];
                    g *= brightness[bright_index * CHANNELS + green];
                    b *= brightness[bright_index * CHANNELS + blue];
                    r = r * (1.0 - fogstrength) + fog[red] * fogstrength;
                    g = g * (1.0 - fogstrength) + fog[green] * fogstrength;
                    b = b * (1.0 - fogstrength) + fog[blue] * fogstrength;
                    glColor3f(r, g, b);
                    glBegin(GL_POINTS);
                    glVertex2i(ray * SCALE-SCALE, start * HSCALE - hposition * HSCALE);
                    glEnd();
                }
            }
        }
        render_infoP++;
    }
}

/*
performs DDA (digital differenciation algorithm) and writes the results to an array
to use in render()
params
return
*/
void DDA() {
    int dofH = DOF;
    int dofV = DOF;
    float deltaYV, deltaYH, deltaXV, deltaXH, rayXV, rayYV;
    float rayXH, rayYH, deltadistH, deltadistV;
    int ray, i;
    bool is_fulltile = false;
    for (i=0;i<mapX*mapY;i++)
        visible[i] = expandable[i] = false;
    visible[(((int)player.posY)>>TILE_POW) * mapX + (((int)player.posX)>>TILE_POW)] = true;
    expandable[(((int)player.posY)>>TILE_POW) * mapX + (((int)player.posX)>>TILE_POW)] = true;
    for (ray = 0; ray<RES;ray++) {
        float distH = 10000.0f;
        float distV = 10000.0f;
        float angle = player.angle - SHIFT + angles[ray];
        radian_change(&angle);
        float correct_fish = player.angle - angle;
        radian_change(&correct_fish);
        correct_fish = cos(correct_fish);

        double Cos = cos(angle);
        double Sin = sin(angle);
        double Tan = tan(angle);
        double invTan = 1 / Tan;

        //up/down
        if (Sin < -0.0001) {
            deltaYH = - TILE;
            deltaXH = - TILE * invTan;
            deltadistH = - TILE / Sin;
            rayYH = ((((int)player.posY)>>TILE_POW)<<TILE_POW) - 0.0001;
            rayXH = player.posX - (player.posY - rayYH) * invTan;
            distH = - (player.posY - rayYH) / Sin;
            dofH = 0;
        }
        else if (Sin > 0.0001) {
            deltaYH = TILE;
            deltaXH = TILE * invTan;
            deltadistH = TILE / Sin;
            rayYH = ((((int)player.posY)>>TILE_POW)<<TILE_POW) + TILE;
            rayXH = player.posX - (player.posY - rayYH) * invTan;
            distH = - (player.posY - rayYH) / Sin;
            dofH = 0;
        }
        else {
            rayXH = player.posX;
            rayYH = player.posY;
            distH = 10000.0f;
            dofH = DOF;
        }

        //left/right
        if (Cos > 0.0001) {
            deltaXV = TILE;
            deltaYV = TILE * Tan;
            deltadistV = TILE / Cos;
            rayXV = ((((int)player.posX)>>TILE_POW)<<TILE_POW) + TILE;
            rayYV = player.posY - (player.posX - rayXV) * Tan;
            distV = - (player.posX - rayXV) / Cos;
            dofV = 0;
        }
        else if (Cos < -0.0001) {
            deltaXV = - TILE;
            deltaYV = - TILE * Tan;
            deltadistV = - TILE / Cos;
            rayXV = ((((int)player.posX)>>TILE_POW)<<TILE_POW) - 0.0001;
            rayYV = player.posY - (player.posX - rayXV) * Tan;
            distV = - (player.posX - rayXV) / Cos;
            dofV = 0;
        }
        else {
            rayXV = player.posX;
            rayYV = player.posY;
            distV = 10000.0f;
            dofV = DOF;
        }
        int raymapXH, raymapYH, raymapH;
        int raymapXV, raymapYV, raymapV;
        bool Hpropagate, Vpropagate;
        door_struct* doorH;
        door_struct* doorV;
        doorH = NULL;
        doorV = NULL;
        while (dofV < DOF || dofH < DOF) {
            is_fulltile = false;
            if (dofH < DOF && distH <= distV) {
                Hpropagate = false;
                raymapXH = (int)(rayXH)>>TILE_POW;
                raymapYH = (int)(rayYH)>>TILE_POW;
                raymapH = raymapXH + raymapYH * mapX;
                if (raymapH >= 0 && raymapH < mapX * mapY) {
                    if (!visible[raymapH])
                        visible[raymapH] = expandable[raymapH]  = true;
                    if (map[raymapH] == 1) {
                        dofH = DOF;
                        doorH = NULL;
                        is_fulltile = true;
                        break;
                    }
                    else if (map[raymapH] == 12 && (int)(rayXH + 0.5 * deltaXH)>>TILE_POW == (int)(rayXH)>>TILE_POW) {
                        doorH = getDoor(raymapXH, raymapYH);
                        if (doorH->x * TILE - doorH->exte + TILE < rayXH + 0.5 * deltaXH) {
                            dofH = DOF;
                            distH += 0.5 * deltadistH;
                            rayXH += 0.5 * deltaXH;
                            rayYH += 0.5 * deltaYH;
                            break;
                        }
                        else
                            Hpropagate = true;
                    }
                    else if (map[raymapH] == 13 && (int)(rayXH + 0.5 * deltaXH)>>TILE_POW == (int)(rayXH)>>TILE_POW) {
                        dofH = DOF;
                        distH += 0.5 * deltadistH;
                        rayXH += 0.5 * deltaXH;
                        rayYH += 0.5 * deltaYH;
                        doorH = NULL;
                        doorV = NULL;
                        break;
                    }
                    else
                        Hpropagate = true;
                }
                else
                    Hpropagate = true;
                if (Hpropagate) {
                    rayXH += deltaXH;
                    rayYH += deltaYH;
                    distH += deltadistH;
                    dofH++;
                }
            }
            else if (dofV < DOF && distV < distH) {
                Vpropagate = false;
                raymapXV = (int)(rayXV)>>TILE_POW;
                raymapYV = (int)(rayYV)>>TILE_POW;
                raymapV = raymapXV + raymapYV * mapX;
                if (raymapV >= 0 && raymapV < mapX * mapY) {
                    if (!visible[raymapV])
                        visible[raymapV] = expandable[raymapV] = true;
                    if (map[raymapV] == 1) {
                        dofV = DOF;
                        doorV = NULL;
                        is_fulltile = true;
                        break;
                    }
                    else if (map[raymapV] == 2 && (int)(rayYV + 0.5 * deltaYV)>>TILE_POW == (int)(rayYV)>>TILE_POW) {
                        doorV = getDoor(raymapXV, raymapYV);
                        if (doorV->y * TILE - doorV->exte + TILE < rayYV + 0.5 * deltaYV) {
                            dofV = DOF;
                            distV += 0.5 * deltadistV;
                            rayXV += 0.5 * deltaXV;
                            rayYV += 0.5 * deltaYV;
                            break;
                        }
                        else
                            Vpropagate = true;
                    }
                    else if (map[raymapV] == 3 && (int)(rayYV + 0.5 * deltaYV)>>TILE_POW == (int)(rayYV)>>TILE_POW) {
                        dofV = DOF;
                        distV += 0.5 * deltadistV;
                        rayXV += 0.5 * deltaXV;
                        rayYV += 0.5 * deltaYV;
                        doorH = NULL;
                        doorV = NULL;
                        break;
                    }
                    else
                        Vpropagate = true;
                }
                else
                    Vpropagate = true;
                if (Vpropagate) {
                    rayXV += deltaXV;
                    rayYV += deltaYV;
                    distV += deltadistV;
                    dofV++;
                }
            }
            else
                break;
        }

        int br_offset = 0;
        render_info[ray].ornament = NULL;
        if (distV <= distH) {
            if (raymapV >= 0 && raymapV < mapX * mapY) {
                if (has_ornament[raymapV]) {
                    render_info[ray].ornament = getOrnament(raymapXV, raymapYV, ornaments);
                    if (!(render_info[ray].ornament->sides[1] && ((int)rayXV) % TILE == 63 || render_info[ray].ornament->sides[3] && ((int)rayXV) % TILE == 0))
                        render_info[ray].ornament = NULL;
                }
            }
            if (doorV == NULL) {
                render_info[ray].texture = &texture_one;
                if (Cos > 0) {
                    render_info[ray].textureX = (int)rayYV % TEXLENGTH ;
                    if (is_fulltile) {
                        if (render_info[ray].textureX>= TILE - LIGHT_GRID)
                            br_offset = -1*mapX*(TILE/LIGHT_GRID);
                        else if (render_info[ray].textureX<=LIGHT_GRID)
                            br_offset = 1*mapX*(TILE/LIGHT_GRID);
                    }
                }
                else {
                    render_info[ray].textureX = TEXLENGTH - 1 - (int)rayYV % TEXLENGTH ;
                    if (is_fulltile) {
                        if (render_info[ray].textureX>= TILE - LIGHT_GRID)
                            br_offset = 1*mapX*(TILE/LIGHT_GRID);
                        else if (render_info[ray].textureX<=LIGHT_GRID)
                            br_offset = -1*mapX*(TILE/LIGHT_GRID);
                    }
                }
            }
            else {
                render_info[ray].textureX = -TEXLENGTH + (int)rayYV % TEXLENGTH + ceil(doorV->exte);
                render_info[ray].texture = &texture_door;
            }
            if (!is_fulltile) {
                if (Cos > 0)
                    br_offset = -1;
                else
                    br_offset = 1;
            }
            render_info[ray].dist = distV;
            render_info[ray].l_height = (float)(pla_sep_scaled * HEIGHT) / distV / correct_fish;
            render_info[ray].rayY = rayYV;
            render_info[ray].rayX = rayXV;
        }
        else {
            if (raymapH >= 0 && raymapH < mapX * mapY) {
                if (has_ornament[raymapH]) {
                    render_info[ray].ornament = getOrnament(raymapXH, raymapYH, ornaments);
                    if (!(render_info[ray].ornament->sides[0] && ((int)rayYH) % TILE == 0 || render_info[ray].ornament->sides[2] && ((int)rayYH) % TILE == 63))
                        render_info[ray].ornament = NULL;
                }
            }
            if (doorH == NULL) {
                render_info[ray].texture = &texture_one;
                if (Sin < 0) {
                    render_info[ray].textureX = (int)rayXH % TEXLENGTH ;
                    if (is_fulltile) {
                        if (render_info[ray].textureX>= TILE - LIGHT_GRID)
                            br_offset = -1;
                        else if (render_info[ray].textureX<=LIGHT_GRID)
                            br_offset = 1;
                    }
                }
                else {
                    render_info[ray].textureX = TEXLENGTH - 1 - (int)rayXH % TEXLENGTH ;
                    if (is_fulltile) {
                        if (render_info[ray].textureX>= TILE - LIGHT_GRID)
                            br_offset = 1;
                        else if (render_info[ray].textureX<=LIGHT_GRID)
                            br_offset = -1;
                    }
                }
            }
            else {
                render_info[ray].textureX = -TEXLENGTH + (int)rayXH % TEXLENGTH + ceil(doorH->exte);
                render_info[ray].texture = &texture_door;
            }
            if (!is_fulltile) {
                if (Sin > 0)
                    br_offset = -1 * mapX * (TILE / LIGHT_GRID);
                else
                    br_offset = 1 * mapX * (TILE / LIGHT_GRID);
            }
            render_info[ray].dist = distH;
            render_info[ray].l_height = (float)(pla_sep_scaled * HEIGHT)/ distH / correct_fish;
            render_info[ray].rayY = rayYH;
            render_info[ray].rayX = rayXH;
        }
        render_info[ray].correct_fish = correct_fish;
        render_info[ray].Cos = Cos;
        render_info[ray].Sin = Sin;

        render_info[ray].bright_index = ((int)render_info[ray].rayY>>LIGHT_POW)*mapX*(TILE/LIGHT_GRID) + ((int)render_info[ray].rayX>>LIGHT_POW) + br_offset;
        if (light_map_length * CHANNELS <= render_info[ray].bright_index || render_info[ray].bright_index < 0)
            render_info[ray].bright_index = 0;
    }
}

/*
handles door opening and closing
params
return
*/
void doorf() {
    door_struct* door;
    door = &doors[0];
    int door_count;
    for (door_count=0;door_count<doornum;door_count++) {
        if (door->exte_rate < 0) {
            door->exte += door->exte_rate * delta_frames / MS_PER_FRAME;
            if (door->exte < 0) {
                door->exte_rate = 0.5;
                door->exte = 0.0;
                door->wait = DOOR_WAIT;
            }
            if (map[door->y * mapX + door->x] == 2) {
                door->door_vert[1].y = door->y * TILE + TILE - ceil(door->exte) + 0.001;
                door->door_vert[2].y = door->door_vert[1].y - 0.002;
            }
            else {
                door->door_vert[1].x = door->x * TILE + TILE - ceil(door->exte) + 0.001;
                door->door_vert[2].x = door->door_vert[1].x - 0.002;
            }
        }
        else if (door->wait > 0)
            door->wait -= 1 * delta_frames / 15;
        else if ((((int)player.posX)>>TILE_POW) != door->x || (((int)player.posY)>>TILE_POW) != door->y) {
            door->exte += door->exte_rate * delta_frames / MS_PER_FRAME;
            if (door->exte > TILE) {
                door->exte_rate = 0.0;
                door->exte = TILE;
            }
            if (map[door->y * mapX + door->x] == 2) {
                door->door_vert[1].y = door->y * TILE + TILE - ceil(door->exte) + 0.001;
                door->door_vert[2].y = door->door_vert[1].y - 0.002;
            }
            else {
                door->door_vert[1].x = door->x * TILE + TILE - ceil(door->exte) + 0.001;
                door->door_vert[2].x = door->door_vert[1].x - 0.002;
            }
        }
        door++;
    }
}

/*
expands the visible bool array by one in all directions
params
return
*/
void expand_visible() {
    int i, j, index;
    int limit = mapX * mapY;
    for (i=0;i<mapY;i++) {
        for (j=0;j<mapX;j++) {
            index = i * mapX + j;
            if (visible[index] && expandable[index]) {
                if (index - 1 < limit && index - 1>0)
                    visible[index - 1] = true;
                if (index + 1 < limit && index + 1>0)
                    visible[index + 1] = true;
                if (index - mapX < limit && index - mapX>0)
                    visible[index - mapX] = true;
                if (index + mapX < limit && index + mapX>0)
                    visible[index + mapX] = true;
            }
        }
    }
}

/*
glut display function, the game loop
params
return
*/
void display() {
    current_frame = glutGet(GLUT_ELAPSED_TIME);
    delta_frames = current_frame - last_frame;
    last_frame = glutGet(GLUT_ELAPSED_TIME);
    if (delta_frames > MS_PER_FRAME * 8)
        delta_frames = 0;
    check_inputs();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    doorf();

    int i, color;
    for (color=0;color<CHANNELS;color++) {
        for (i=0;i< light_map_length;i++) {
            brightness[i * CHANNELS + color] = stationary[i * CHANNELS + color];
            if (brightness[i * CHANNELS + color] > 1)
                brightness[i * CHANNELS + color] = 1;
        }
    }

    float dlight_map[light_map_length * CHANNELS];
    for (i=0;i<ac_light_num;i++) {
        if (ac_lights[i].source->state) {
            gen_light(*ac_lights[i].source, dlight_map);
            combine_light(brightness, dlight_map);
            ac_lights[i].direction += ac_lights[i].deltadir * delta_frames + PI;
            radian_change(&ac_lights[i].direction);
            movef(ac_lights[i].direction, &(ac_lights[i].source->posX), &(ac_lights[i].source->posY), ac_lights[i].deltamove);
            ac_lights[i].direction -= PI;
            radian_change(&ac_lights[i].direction);
        }
    }
    for (i=0;i<sprite_num;i++) {
        sprites[i].direction += sprites[i].deltadir * delta_frames + PI;
        radian_change(&sprites[i].direction);
        movef(sprites[i].direction, &sprites[i].posX, &sprites[i].posY, sprites[i].deltamove);
        sprites[i].direction -= PI;
        radian_change(&sprites[i].direction);
    }

    DDA();
    expand_visible();
    render();
    if (show_map)
        draw_map();
    glutSwapBuffers();
    oldkeys = newkeys;
}

/*
detects when keys are pressed
params
key - key code
x - mouse x location
y - mouse y location
return
*/
void keydown(unsigned char key, int x, int y) {
    if (key == 'w')
        newkeys.w = 1;
    if (key == 'a')
        newkeys.a = 1;
    if (key == 's')
        newkeys.s = 1;
    if (key == 'd')
        newkeys.d = 1;
    if (key == 'q')
        newkeys.q = 1;
    if (key == 'e')
        newkeys.e = 1;
    if (key == 'm')
        newkeys.m = 1;
    if (key == 'f')
        newkeys.f = 1;
    if (key == ' ')
        newkeys.space = 1;
    if (key == 'p')
        newkeys.p = 1;
    if (key == 'R')
        newkeys.R = 1;
}

/*
detects when keys are released
params
key - key code
x - mouse x location
y - mouse y location
return
*/
void keyup(unsigned char key, int x, int y) {
    if (key == 'w')
        newkeys.w = 0;
    if (key == 'a')
        newkeys.a = 0;
    if (key == 's')
        newkeys.s = 0;
    if (key == 'd')
        newkeys.d = 0;
    if (key == 'q')
        newkeys.q = 0;
    if (key == 'e')
        newkeys.e = 0;
    if (key == 'm')
        newkeys.m = 0;
    if (key == 'f')
        newkeys.f = 0;
    if (key == ' ')
        newkeys.space = 0;
    if (key == 'p')
        newkeys.p = 0;
    if (key == 'R')
        newkeys.R = 0;
}

/*
generates an array of submap positions in the map file
params
*fptr - file pointer
return
*/
void gen_submap_arr(FILE* fptr) {
    int submapn_arr[1];
    parsel(fptr, 1, submapn_arr, -1);
    submapn = submapn_arr[0];
    submapindex = malloc(sizeof(long int) * submapn);
    if (submapindex == NULL) {
        exit(1);
    }
    int lc, c, i;
    int maph[3];
    fseek(fptr, 0, SEEK_SET);
    i = 0;
    lc = getc(fptr);
    while (c != EOF && i < submapn) {
        c = getc(fptr);
        if ((lc == 0x0D || lc == '\n') && c == 'm')
            submapindex[i++] = ftell(fptr);
        lc = c;
    }
}

/*
generates and inicialises the map, sprites, lights, ornaments, player from the .map file
params
*fptr - file pointer
mapindex - index of the submap in the .map file
return
*/
void initmap(FILE* fptr, int mapindex) {
    fseek(fptr, submapindex[mapindex], SEEK_SET);
    submap_exit = false;
    exit_level = -1;
    int mapXY[3];
    parsel(fptr, 3, mapXY, -1);
    mapX = mapXY[1];
    mapY = mapXY[2];
    light_map_length = mapX * mapY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID);
    map = malloc(sizeof(int) * mapX * mapY);
    exit_level++;
    if (map == NULL)
        exit(1);
    has_ornament = calloc(mapX * mapY, sizeof(bool));
    exit_level++;
    if (has_ornament == NULL)
        exit(1);
    visible = calloc(mapX * mapY, sizeof(bool));
    exit_level++;
    if (visible == NULL)
        exit(1);
    expandable = calloc(mapX * mapY, sizeof(bool));
    exit_level++;
    if (expandable == NULL)
        exit(1);
    stationary = calloc(light_map_length * CHANNELS, sizeof(float));
    exit_level++;
    if (stationary == NULL)
        exit(1);
    brightness = calloc(light_map_length * CHANNELS, sizeof(float));
    exit_level++;
    if (brightness == NULL)
        exit(1);
    int mapline[mapX];
    int i, j, ia, ja, neib_num;
    bool neib_b[4];
    vnum = 0;
    for (i=0;i<mapY;i++) {
        parsel(fptr, mapX, mapline,-1);
        for (j=0;j<mapX;j++) {
            map[i*mapX + j] = mapline[j];
    		if ((i-1) * mapX + j - 1 >= 0) {
                neib_num = 0;
                for (ia=0;ia<2;ia++) {
                    for (ja=0;ja<2;ja++) {
                        if (map[(i+ia - 1) * mapX + j +ja - 1] == 1)
                            neib_num++;
                    }
                }
                if (neib_num != 4 && neib_num != 0)
                    vnum++;
                if (neib_num == 1 || neib_num == 3)
                    vnum++;
            }
    		if (map[i * mapX + j] == 2) {
			    doornum++;
                vnum+=3;
            }
            else if (map[i * mapX + j] == 3)
                vnum+=2;
        }
    }
    doors = malloc(sizeof(*doors) * doornum);
    exit_level++;
    if (doors == NULL)
        exit(1);
    vert = malloc((vnum + 4) * sizeof(fpoint));
    exit_level++;
    if (doors == NULL)
        exit(1);
    doornum = 0;
    float vshiftX, vshiftY;
    vnum = 0;
    for (i=0;i<mapY;i++) {
    	for (j=0;j<mapX;j++) {
    		if (map[i * mapX + j] == 2) {
                if (map[i * mapX + j - 1] == 1 && map[i * mapX + j + 1] == 1) {
                    map[i * mapX + j] = 12;
                    doors[doornum].door_vert[0].x = j * TILE + TILE - 0.001; //not open
                    doors[doornum].door_vert[0].y = i * TILE + TILE / 2;
                    doors[doornum].door_vert[1].x = j * TILE + 0.001; //x not open
                    doors[doornum].door_vert[1].y = i * TILE + TILE / 2;
                    doors[doornum].door_vert[2].x = j * TILE - 0.001; //x opening
                    doors[doornum].door_vert[2].y = i * TILE + TILE / 2;
                }
                else {
                    doors[doornum].door_vert[0].x = j * TILE + TILE / 2;
                    doors[doornum].door_vert[0].y = i * TILE + TILE - 0.001; //not open
                    doors[doornum].door_vert[1].x = j * TILE + TILE / 2;
                    doors[doornum].door_vert[1].y = i * TILE + 0.001; //y not open
                    doors[doornum].door_vert[2].x = j * TILE + TILE / 2;
                    doors[doornum].door_vert[2].y = i * TILE - 0.001; //y opening
                }
    			doors[doornum].x = j;
			    doors[doornum].y = i;
			    doors[doornum].exte = TILE;
			    doors[doornum].exte_rate = 0.0;
			    doors[doornum].wait = 0;
			    doornum++;
            }
    		else if (map[i * mapX + j] == 3) {
                if (map[i * mapX + j - 1] == 1 && map[i * mapX + j + 1] == 1) {
                    map[i * mapX + j] = 13;
                    vert[vnum].x = j * TILE + 0.001;
                    vert[vnum++].y = i * TILE + TILE / 2;
                    vert[vnum].x = j * TILE + TILE - 0.001;
                    vert[vnum++].y = i * TILE + TILE / 2;
                }
                else {
                    vert[vnum].x = j * TILE + TILE / 2;
                    vert[vnum++].y = i * TILE + 0.001;
                    vert[vnum].x = j * TILE + TILE / 2;
                    vert[vnum++].y = i * TILE + TILE - 0.001;
                }
			}
            if ((i-1) * mapX + j - 1 >= 0) {
                neib_num = 0;
                neib_b[0] = 0;
                neib_b[1] = 0;
                neib_b[2] = 0;
                neib_b[3] = 0;
                for (ia=0;ia<2;ia++) {
                    for (ja=0;ja<2;ja++) {
                        if (map[(i+ia-1) * mapX + j +ja-1] == 1) {
                            neib_b[ia * 2 + ja] = 1;
                            neib_num++;
                        }
                    }
                }
                vshiftX = 0.001 * (neib_b[0] + neib_b[2] - neib_b[1] - neib_b[3]);
                vshiftY = 0.001 * (neib_b[0] + neib_b[1] - neib_b[2] - neib_b[3]);
                if (neib_num == 1 || neib_num == 2) {
                    vert[vnum].x = j * TILE + vshiftX;
                    vert[vnum++].y = i * TILE + vshiftY;
                    if (neib_num == 1) {
                        vert[vnum].x = j * TILE - vshiftX;
                        vert[vnum++].y = i * TILE - vshiftY;
                    }
                }
                if (neib_num == 3) {
                    vert[vnum].x = j * TILE - vshiftX;
                    vert[vnum++].y = i * TILE + vshiftY;
                    vert[vnum].x = j * TILE + vshiftX;
                    vert[vnum++].y = i * TILE- vshiftY;
                }
            }
		}
	}
    vnum+=4;

    int playerXYA[3];
    int player_tmpX;
    parsel(fptr, 3, playerXYA, -1);
    if (firstmap) {
        player.posX = playerXYA[0] * TILE + TILE / 2;
        player.posY = playerXYA[1] * TILE + TILE / 2;
        player.angle = playerXYA[2] / 50.0 * PI;
    }
    else {
        player.posX = (int)(player.posX) % TILE - TILE / 2;
        player.posY = (int)(player.posY) % TILE - TILE / 2;
        player_tmpX = player.posX;
        player.posX = player.posX * cos(current_teleport.rotation) - player.posY * sin(current_teleport.rotation) + TILE / 2;
        if (player.posX < 0)
            player.posX = 0;
        else if (player.posX >= TILE)
            player.posX = TILE - 1;
        player.posY = player_tmpX * sin(current_teleport.rotation) + player.posY * cos(current_teleport.rotation) + TILE / 2;
        if (player.posY < 0)
            player.posY = 0;
        else if (player.posY >= TILE)
            player.posY = TILE - 1;
        player.posX += current_teleport.exitX * TILE;
        player.posY += current_teleport.exitY * TILE;
        player.angle -=  current_teleport.rotation;
        radian_change(&player.angle);
    }
    firstmap = false;
    int intamb[CHANNELS];
    parsel(fptr, 3, intamb, 0);
    int fogse[2];
    parsel(fptr, 2, fogse, -1);
    DOF = (float)fogse[1];
    fade_start = (float)fogse[0];
    delta_fade = DOF - fade_start;
    int intfog[CHANNELS];
    parsel(fptr, 3, intfog, 0);
    float amb[CHANNELS];
    for (i=0;i<CHANNELS;i++)
        fog[i] = intfog[i] / 255.0;
    for (i=0;i<light_map_length;i++) {
        stationary[i * CHANNELS + red] += intamb[red] / 255.0;
        stationary[i * CHANNELS + green] += intamb[green] / 255.0;
        stationary[i * CHANNELS + blue] += intamb[blue] / 255.0;
    }

    int lightnum[1];
    parsel(fptr, 1, lightnum, -1);
    stat_light_num = lightnum[0];
    stat_lights = malloc(sizeof(*stat_lights) * stat_light_num);
    exit_level++;
    if (stat_lights == NULL)
        exit(1);
    int lightslocal[7];
    for (i=0;i<stat_light_num;i++) {
        parsel(fptr, 7, lightslocal, 4);
        stat_lights[i].is_static = true;
        stat_lights[i].posX = lightslocal[0];
        stat_lights[i].posY = lightslocal[1];
        stat_lights[i].state = lightslocal[2];;
        stat_lights[i].intens = lightslocal[3];
        stat_lights[i].r = lightslocal[4] / 255.0;
        stat_lights[i].g = lightslocal[5] / 255.0;
        stat_lights[i].b = lightslocal[6] / 255.0;
        if (stat_lights[i].state) {
            gen_light(stat_lights[i], brightness);
            combine_light(stationary, brightness);
        }
    }

    int acl_num[1];
    parsel(fptr, 1, acl_num, -1);
    ac_light_num = acl_num[0];

    ac_lights = malloc(sizeof(*ac_lights) * ac_light_num);
    exit_level++;
    if (ac_lights == NULL)
        exit(1);
    lights = malloc(sizeof(*lights) * ac_light_num);
    exit_level++;
    if (lights == NULL)
        exit(1);
    int acl_info[10];
    for (i=0;i<ac_light_num;i++) {
        parsel(fptr, 10, acl_info, 7);
        ac_lights[i].direction = acl_info[3] / 50.0 * PI;
        ac_lights[i].deltadir = acl_info[4] / 50000.0 * PI;
        ac_lights[i].deltamove = acl_info[5] / 100.0;
        ac_lights[i].source = &lights[i];
        ac_lights[i].source->is_static = false;
        ac_lights[i].source->posX = acl_info[0];
        ac_lights[i].source->posY = acl_info[1];
        ac_lights[i].source->state = acl_info[2];
        ac_lights[i].source->intens = acl_info[6];
        ac_lights[i].source->r = acl_info[7] / 255.0;
        ac_lights[i].source->g = acl_info[8] / 255.0;
        ac_lights[i].source->b = acl_info[9] / 255.0;
    }

    int spr_num[1];
    parsel(fptr, 1, spr_num, -1);
    sprite_num = spr_num[0];
    sprites = malloc(sizeof(*sprites) * sprite_num);
    exit_level++;
    if (sprites == NULL)
        exit(1);
    int spriteinfo[6];
    for (i=0;i<sprite_num;i++) {
        parsel(fptr, 6, spriteinfo, -1);
        sprites[i].posX = spriteinfo[0];
        sprites[i].posY = spriteinfo[1];
        sprites[i].direction = spriteinfo[2] / 50.0 * PI;
        sprites[i].deltadir = spriteinfo[3] / 50000.0 * PI;
        sprites[i].deltamove = spriteinfo[4] / 100.0;
        if (spriteinfo[5] == 0)
            sprites[i].texture = &texture_one;
        else
            sprites[i].texture = &texture_missing;
    }

    int teleportn_arr[1];
    parsel(fptr, 1, teleportn_arr, -1);
    teleportnumber = teleportn_arr[0];
    int tele_arr[6];
    teleport_arr = malloc(sizeof(teleport_info) * teleportnumber);
    exit_level++;
    if (teleport_arr == NULL)
        exit(1);
    for (i=0;i<teleportnumber;i++) {
        parsel(fptr, 7, tele_arr, -1);
        teleport_arr[i].subindex = tele_arr[0];
        teleport_arr[i].interact = tele_arr[1];
        teleport_arr[i].enterX = tele_arr[2];
        teleport_arr[i].enterY = tele_arr[3];
        teleport_arr[i].exitX = tele_arr[4];
        teleport_arr[i].exitY = tele_arr[5];
        teleport_arr[i].rotation = tele_arr[6] / 50.0 * PI;
    }

    /*
    num of ornaments
    x, y, side  0  , num of links, linkid(s)(lightnumber)
              3   1
                2
    */
    int ornam_num[1];
    int ornamentX[2];
    int ornamentY[2];
    int localsides[2];
    int k;
    parsel(fptr, 1, ornam_num, -1);
    ornament_struct local_ornaments[ornam_num[0] + doornum * 2];
    ornament_num = 0;
    ornament_struct *ornament_cur = &local_ornaments[0];
    ornament_struct *ornament_old;
    for (i=0;i<doornum;i++) {
        if (map[doors[i].y * mapX + doors[i].x] == 2) {
            ornamentX[0] = doors[i].x;
            ornamentY[0] = doors[i].y-1;
            localsides[0] = 2;
            ornamentX[1]= doors[i].x;
            ornamentY[1]= doors[i].y+1;
            localsides[1] = 0;
        }
        else {
            ornamentX[0] = doors[i].x - 1;
            ornamentY[0] = doors[i].y;
            localsides[0] = 1;
            ornamentX[1]= doors[i].x + 1;
            ornamentY[1]= doors[i].y;
            localsides[1] = 3;
        }
        for (j=0;j<2;j++) { 
                ornament_old = getOrnament(ornamentX[j], ornamentY[j], local_ornaments);
                if (ornament_old == NULL) {
                    has_ornament[ornamentY[j] * mapX + ornamentX[j]] = true;
                    for (k=0;k<4;k++) {
                        ornament_cur->sides[k] = false;
                        ornament_cur->num_of_indexes[k] = 0;
                        ornament_cur->teleport[k] = -1;
                    }
                    ornament_cur->texture = &texture_missing;
                    ornament_cur->posX = ornamentX[j];
                    ornament_cur->posY = ornamentY[j];
                    ornament_cur->sides[localsides[j]] = true;
                    ornament_num++;
                    ornament_cur++;
                }
                else
                    ornament_old->sides[localsides[j]] = true;
        }
    }
    int local_ornament_arr[22]; // TODO: index 4 is texture;
    for (i=0;i<ornam_num[0];i++) {
        parsel(fptr, 22, local_ornament_arr, -1);
        ornament_old = getOrnament(local_ornament_arr[0], local_ornament_arr[1], local_ornaments);
        if (ornament_old == NULL) {
            has_ornament[local_ornament_arr[1] * mapX + local_ornament_arr[0]] = true;
            for (k=0;k<4;k++) {
                ornament_cur->sides[k] = false;
                ornament_cur->num_of_indexes[k] = 0;
                ornament_cur->teleport[k] = -1;
            }
            ornament_cur->texture = &texture_missing;
            ornament_cur->num_of_indexes[local_ornament_arr[2]] = local_ornament_arr[5];
            ornament_cur->posX = local_ornament_arr[0];
            ornament_cur->posY = local_ornament_arr[1];
            ornament_cur->sides[local_ornament_arr[2]] = true;
            ornament_cur->teleport[local_ornament_arr[2]] = local_ornament_arr[3];
            for (j=0;j<16 && j<local_ornament_arr[5];j++)
                ornament_cur->lights_index[j][local_ornament_arr[2]] = local_ornament_arr[j+6];
            ornament_num++;
            ornament_cur++;
        }
        else {
            ornament_old->num_of_indexes[local_ornament_arr[2]] = local_ornament_arr[5];
            ornament_old->sides[local_ornament_arr[2]] = true;
            ornament_old->teleport[local_ornament_arr[2]] = local_ornament_arr[3];
            for (j=0;j<16 && j<local_ornament_arr[5];j++)
                ornament_old->lights_index[j][local_ornament_arr[2]] = local_ornament_arr[j+6];
        }
    }
    ornaments = malloc(sizeof(*ornaments) * ornament_num);
    exit_level++;
    if (ornaments == NULL)
        exit(1);
    for (i=0;i<ornament_num;i++)
        ornaments[i] = local_ornaments[i];
}

/*
inicialises textures, freeglut and the first map
params
return
*/
void init() {
    FILE* fptr;
    firstmap = true;
    mapf = fopen("map.map", "r");
    if (mapf == NULL) {
        printf("No map file found");
        exit(1);
    }
    genltex(LTEX_S, ltexture);
    gen_submap_arr(mapf);
    initmap(mapf, curr_submap_index);
    fptr = NULL;
    glClearColor(0.3,0.3,0.3,0);
    gluOrtho2D(0,(RES - 2)*SCALE,(HEIGHT - 2) * HSCALE,0);

    genmissing(texture_missing);

    fptr = fopen("tile.ppm", "r");
    if (fptr != NULL) {
        if (parse(fptr, texture_one))
            genmissing(texture_one);
    }
    else {
        printf("Failed to open file tile.ppm\n");
        genmissing(texture_one);
    }
    fptr = fopen("door.ppm", "r");
    if (fptr != NULL) {
        if (parse(fptr, texture_door))
            genmissing(texture_door);
    }
    else {
        printf("Failed to open file door.ppm\n");
        genmissing(texture_door);
    }
    fptr = fopen("floor.ppm", "r");
    if (fptr != NULL) {
        if (parse(fptr, texture_floor))
            genmissing(texture_floor);
    }
    else {
        printf("Failed to open file floor.ppm\n");
        genmissing(texture_floor);
    }
    fptr = fopen("ceil.ppm", "r");
    if (fptr != NULL) {
        if (parse(fptr, texture_ceil))
            genmarble(texture_ceil);
    }
    else {
        printf("Failed to open file ceil.ppm\n");
        genmarble(texture_ceil);
    }
    float base_angle = 0.5 * PI - SHIFT;
    float baseCos = cos(base_angle);
    float side = RES * sin(base_angle) / sin(FOV);
    pla_sep = side * sin(base_angle);
    pla_sep_scaled = pla_sep / MAGIC;
    floor_const = PLAYER_HEIGHT * pla_sep_scaled;
    int i;
    for (i=0; i<RES; i++)
        angles[i] = acos((side - i * baseCos) / sqrt(SQR(side) + SQR(i) - 2 * side * i * baseCos));
}

/*
declares glut functions, calls init
params
argc - number of args
*argv[] - arguments
return
*/
int main(int argc, char* argv[]) {
    if (atexit(exitmap))
        return EXIT_FAILURE;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize((RES - 2) * SCALE, (HEIGHT - 2) * HSCALE);
    glutCreateWindow("Raycaster");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keydown);
    glutKeyboardUpFunc(keyup);
    glutMainLoop();
    return EXIT_SUCCESS;
}
