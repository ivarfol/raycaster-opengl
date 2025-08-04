#include <stdio.h>
#include <GL/glut.h>
#include <math.h>
#include <stdbool.h>

#define PI 3.1415926535
#define PLAYER_SPEED 5
#define RES 256
#define SCALE 4
#define FOV 0.5 * PI
#define SHIFT (FOV / 2)

//#define DOORN 7

#define LIGHT_NUM 1
#define SPRITE_NUM 1
#define MAP_SCALE 16
#define LIGHT_GRID 2

#define LIGHT_POW 1
#define TILE_POW 6

#define MIN_LIGHT_DIST 128

#define TEXWIDTH 64
#define TEXHEIGHT 64
#define CHANNELS 3
#define TILE 64

#define PLHEIGHT 32

#define FLOOR_SCALE 1
#define HEIGHT 512
//#define MAPX 10
//#define MAPY 13

#define MIN_BRIGHTNESS 0.01

#define LIGHT_POS (4 * TILE) //light source

//#define LIGHT_MAP_L (MAPX * MAPY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID))

/*
#define AMBIENT_R 0.1f
#define AMBIENT_G 0.1f
#define AMBIENT_B 0.1f
*/

/*
#define FADES 5.0
#define DFADE 3.0
#define FOGR 0.1
#define FOGG 0.1
#define FOGB 0.1

#define DOF (FADES + DFADE)
*/

#define LDOF 20

float texture_one[TEXWIDTH * TEXHEIGHT][CHANNELS];
float texture_missing[TEXWIDTH * TEXHEIGHT][CHANNELS];
extern int parse(FILE* fptr, float texture[][CHANNELS]);
extern void parsel(FILE* fptr, int num, int out[], int colo_pos);

enum { red, green, blue };

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
    unsigned shift : 1;
    unsigned p : 1;
    unsigned l : 1;
} keys;
keys oldkeys;
keys newkeys;

typedef struct {
    int x, y, wait;
    float exte, exte_rate;
} door_struct;
door_struct *doors;

typedef struct {
    bool isdoor;
    float rayX;
    float rayY;
    float l_height;
    float textureX;
    float correct_fish;
    double Cos;
    double Sin;
    float dist;
} line_out_info;
line_out_info render_info[RES];

typedef struct {
    float posX;
    float posY;
    float intens;
    unsigned is_static : 1;
    float r;
    float g;
    float b;
} light_source;
light_source lights[LIGHT_NUM];
//float light_maps[LIGHT_NUM][LIGHT_MAP_L + 4 * MAPX * MAPY][CHANNELS] = {0};

typedef struct {
    float posX;
    float posY;
    float direction;
    unsigned is_static : 1;
    light_source* source;
} sprite_strct;
sprite_strct sprites[SPRITE_NUM];

float playerX, playerY, playerAngle;
bool show_map = true;
float move_direction_v, move_direction_h;
//double floor_const = PLHEIGHT * tan(SHIFT) * RES / 2 * SCALE; // aspect 1/2
double floor_const = HEIGHT * RES / 2 / 4/ tan(0.5*PI -SHIFT) ; // aspect 1/2
float current_frame = 0.0;
float delta_frames, last_frame;

float angles[RES];

/*
int map[MAPX * MAPY] =
{
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 1, 0, 2, 0, 1,
    1, 0, 1, 0, 0, 2, 0, 1, 0, 1,
    1, 3, 1, 0, 0, 1, 0, 5, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 5, 0, 1,
    1, 0, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 5, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 5, 0, 1,
    1, 4, 1, 4, 1, 3, 1, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
*/

//bool visible[MAPX * MAPY] = { false };
//bool expandable[MAPX * MAPY] = { false };

//float stationary[LIGHT_MAP_L + MAPX *MAPY * 4][CHANNELS] = {0};
//float brightness[LIGHT_MAP_L + MAPX *MAPY * 4][CHANNELS] = {0};
//float player_light[LIGHT_MAP_L + MAPX *MAPY * 4][CHANNELS];

int *mapX, *mapY, *map, LIGHT_MAP_L, FULL_LML;
int *intamb, *fogs, *foge, *intfog;

int MAPX, MAPY;
int DOORN = 0;

float amb[CHANNELS], fog[CHANNELS];
float *stationary, *brightness, *light_maps;

bool *visible, *expandable;

float DOF, FADES, DFADE;

void radian_change(float *a) {
    if (*a > 2 * PI)
        *a -= 2 * PI;
    else if (*a < 0)
        *a += 2 * PI;
    if (*a > 2 * PI || *a < 0)
        radian_change(a);
}

door_struct* getDoor(int x, int y) {
    int i;
    for (i=0;doors[i].x != x || doors[i].y != y;i++);
    return(&doors[i]);
}



void exitmap() {
    free(map);
    map = NULL;
    free(intamb);
    intamb = NULL;
    free(intfog);
    intfog = NULL;
}

bool path_free(float posX, float posY, int tileX, int tileY, double angle, double dist, int corner) {
    float r, g, b;
    int dofH = LDOF;
    int dofV = LDOF;
    float deltaYV, deltaYH;
    float deltaXV, deltaXH;
    float rayXV, rayYV;
    float rayXH, rayYH;
    float deltadistH, deltadistV;
    int ray, i;
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
    /*
    if (distH < distV) { 
        if ((distH + deltadistH / (TILE / LIGHT_GRID) >= dist && distH != 10000.0f))
            return true;
    }
    else if ((distV + deltadistV / (TILE / LIGHT_GRID) >= dist && distV != 10000.0f))
        return true;
    */
    
    float deltadist_mult = TILE / LIGHT_GRID;
    if (corner >= 0)
        deltadist_mult *= 4;
    int raymapXH, raymapYH, raymapH;
    int raymapXV, raymapYV, raymapV;
    door_struct* doorH;
    door_struct* doorV;
    doorH = NULL;
    doorV = NULL;
    while (dofV < LDOF || dofH < LDOF) {
        if (dofH < LDOF && distH <= distV) {
            raymapXH = (int)(rayXH)>>TILE_POW;
            raymapYH = (int)(rayYH)>>TILE_POW;
            if ((distH + deltadistH / deltadist_mult >= dist && distH != 10000.0f))
                return true;
            raymapH = raymapXH + raymapYH * MAPX;
            if (raymapH > 0 && raymapH < MAPX * MAPY && map[raymapH] == 1) {
                dofH = LDOF;
                doorH = NULL;
                break;
            }
            else {
                rayXH += deltaXH;
                rayYH += deltaYH;
                distH += deltadistH;
                dofH++;
            }
        }
        else if (dofV < LDOF && distV < distH) {
            raymapXV = (int)(rayXV)>>TILE_POW;
            raymapYV = (int)(rayYV)>>TILE_POW;
            if ((distV + deltadistV / deltadist_mult >= dist && distV != 10000.0f))
                return true;
            raymapV = raymapXV + raymapYV * MAPX;
            if (raymapV > 0 && raymapV < MAPX * MAPY && map[raymapV] == 1) {
                dofV = LDOF;
                doorV = NULL;
                break;
            }
            else {
                rayXV += deltaXV;
                rayYV += deltaYV;
                distV += deltadistV;
                dofV++;
            }
        }
        else
            break;
    }
    return false;
}

void gen_light(light_source light, float light_map[]) {
//int lightX, int lightY, float intens, float bright_arr[][CHANNELS], float r, float g, float b, bool is_static;
    int startX, endX;
    int startY, endY;
    float max_dist = sqrt(light.intens / MIN_BRIGHTNESS);
    startX = (int)(light.posX- max_dist)>>LIGHT_POW;
    if (startX < 0)
        startX = 0;
    endX = (int)(light.posX+ max_dist)>>LIGHT_POW;
    if (endX > MAPX * (TILE / LIGHT_GRID))
        endX = MAPX * (TILE / LIGHT_GRID);

    startY = (int)(light.posY- max_dist)>>LIGHT_POW;
    if (startY < 0)
        startY = 0;
    endY = (int)(light.posY+ max_dist)>>LIGHT_POW;
    if (endY > MAPY * (TILE / LIGHT_GRID))
        endY = MAPY * (TILE / LIGHT_GRID);

    int tileY, tileX;
    float distX, distY;
    float bright;
    int i, color;
    for (i=0;i<LIGHT_MAP_L;i++) {
        for (color=0;color<CHANNELS;color++) {
            light_map[i * CHANNELS + color] = 0;
        }
    }
    double angle, dist;
    float mult;
    int bright_index;
    int lightXremain, lightYremain;
    int corner;
    for (tileY = startY;tileY<endY;tileY++) {
        for (tileX = startX;tileX<endX;tileX++) {
            if (visible[(tileY>>(TILE_POW-LIGHT_POW))*MAPX + (tileX>>(TILE_POW-LIGHT_POW))] || light.is_static == 1) {
                corner = -1;
                bright_index = (tileY) * MAPX * (TILE / LIGHT_GRID) + tileX;
                lightXremain = tileX % (TILE / LIGHT_GRID);
                lightYremain = tileY % (TILE / LIGHT_GRID);
                if (map[(tileY>>(TILE_POW - LIGHT_POW))*MAPX + (tileX>>(TILE_POW - LIGHT_POW))]==1) {
                    //btm rght
                    if (lightXremain == (TILE / LIGHT_GRID) - 1 && lightYremain == (TILE / LIGHT_GRID) - 1) {
                        corner = 3;
                        distX = (tileX + 0.875) * LIGHT_GRID - light.posX;
                        distY = (tileY + 0.125) * LIGHT_GRID - light.posY;
                        //bright_index = LIGHT_MAP_L + 4 * (tileY>>(TILE_POW - LIGHT_POW)) * MAPX + 4 * (tileX>>(TILE_POW - LIGHT_POW)) + 3;
                    }

                    //btm lft
                    else if (lightXremain == 0 && lightYremain == (TILE / LIGHT_GRID) - 1) {
                        corner = 2;
                        distX = (tileX + 0.125) * LIGHT_GRID - light.posX;
                        distY = (tileY + 0.125) * LIGHT_GRID - light.posY;
                        //bright_index = LIGHT_MAP_L + 4 * (tileY>>(TILE_POW - LIGHT_POW)) * MAPX + 4 * (tileX>>(TILE_POW - LIGHT_POW)) + 2;
                    }

                    //tp rght
                    else if (lightXremain == (TILE / LIGHT_GRID) - 1 && lightYremain == 0) {
                        corner = 1;
                        distX = (tileX + 0.875) * LIGHT_GRID - light.posX;
                        distY = (tileY + 0.875) * LIGHT_GRID - light.posY;
                        //bright_index = LIGHT_MAP_L + 4 * (tileY>>(TILE_POW - LIGHT_POW)) * MAPX + 4 * (tileX>>(TILE_POW - LIGHT_POW)) + 1;
                    }

                    //tp lft
                    else if (lightXremain == 0 && lightYremain == 0) {
                        corner = 0;
                        distX = (tileX + 0.125) * LIGHT_GRID - light.posX;
                        distY = (tileY + 0.875) * LIGHT_GRID - light.posY;
                        //bright_index = LIGHT_MAP_L + 4 * (tileY>>(TILE_POW - LIGHT_POW)) * MAPX + 4 * (tileX>>(TILE_POW - LIGHT_POW));
                    }
                    else {
                        distX = (tileX + 0.5) * LIGHT_GRID - light.posX;
                        distY = (tileY + 0.5) * LIGHT_GRID - light.posY;
                    }
                }
                else {
                    distX = (tileX + 0.5) * LIGHT_GRID - light.posX;
                    distY = (tileY + 0.5) * LIGHT_GRID - light.posY;
                }
                dist = sqrt(distX * distX + distY * distY);
                angle = atan(distY / distX);
                if (distX < 0)
                    angle += PI;
                mult = 0.25;
                if (path_free(light.posX, light.posY, tileX>>(TILE_POW - LIGHT_POW), tileY>>(TILE_POW - LIGHT_POW), angle, dist, corner))
                    mult = 1;
                bright = light.intens / (distX * distX + distY * distY) * mult;
                if (bright > 1)
                    bright = 1;
                else if (bright < 0)
                    bright = 1;
                light_map[bright_index * CHANNELS + red] = bright * light.r;
                light_map[bright_index * CHANNELS + green] = bright * light.g;
                light_map[bright_index * CHANNELS + blue] = bright * light.b;

                if (corner>=0) {
                    if (corner == 3) {
                        distX = (tileX + 0.125) * LIGHT_GRID - light.posX;
                        distY = (tileY + 0.875) * LIGHT_GRID - light.posY;
                    }

                    //btm lft
                    else if (corner == 2) {
                        distX = (tileX + 0.875) * LIGHT_GRID - light.posX;
                        distY = (tileY + 0.875) * LIGHT_GRID - light.posY;
                    }

                    //tp rght
                    else if (corner == 1) {
                        distX = (tileX + 0.125) * LIGHT_GRID - light.posX;
                        distY = (tileY + 0.125) * LIGHT_GRID - light.posY;
                    }

                    //tp lft
                    else if (corner == 0) {
                        distX = (tileX + 0.875) * LIGHT_GRID - light.posX;
                        distY = (tileY + 0.125) * LIGHT_GRID - light.posY;
                    }
                    bright_index = LIGHT_MAP_L + 4 * (tileY>>(TILE_POW - LIGHT_POW)) * MAPX + 4 * (tileX>>(TILE_POW - LIGHT_POW)) + corner;
                    dist = sqrt(distX * distX + distY * distY);
                    angle = atan(distY / distX);
                    if (distX < 0)
                        angle += PI;
                    mult = 0.25;
                    if (path_free(light.posX, light.posY, tileX>>(TILE_POW - LIGHT_POW), tileY>>(TILE_POW - LIGHT_POW), angle, dist, corner))
                        mult = 1;

                    bright = light.intens / (distX * distX + distY * distY) * mult;
                    if (bright > 1)
                        bright = 1;
                    else if (bright < 0)
                        bright = 1;
                    light_map[bright_index * CHANNELS + red] = bright * light.r;
                    light_map[bright_index * CHANNELS + green] = bright * light.g;
                    light_map[bright_index * CHANNELS + blue] = bright * light.b;
                }
            }
        }
    }
    /*
    if (newkeys.l) {
        for (tileY = 0; tileY < MAPY * (TILE / LIGHT_GRID);tileY++) {
            for (tileX = 0; tileX < MAPX * (TILE / LIGHT_GRID);tileX++) {
                printf("%0.2f ", light.light_map[tileY  * CHANNELS + MAPY * (TILE / LIGHT_GRID) + tileX]);
            }
            printf("\n");
        }
        printf("\n");
    }
    */
}

void combine_light(float light_one[], float light_two[]) {
    int i, color;
    for (i=0;i< LIGHT_MAP_L + 4 * MAPX * MAPY;i++) {
        for (color=0;color<CHANNELS;color++) {
            light_one[i * CHANNELS + color] += light_two[i * CHANNELS + color];
            if (light_one[i * CHANNELS + color] > 1)
                light_one[i * CHANNELS + color] = 1;
            //printf("%0.2f ", light_one[i][color]);
        }
        //printf("\n");
    }
}

void drawMap() {
    int x, y, tileX, tileY;
    for (y=0;y<MAPY;y++) {
        for (x=0;x<MAPX;x++) {
            if (map[y*MAPX+ x] == 1) {
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
            else if (map[y*MAPX + x] == 2) {
                tileX = (x + 0.5) * MAP_SCALE;
                tileY = y * MAP_SCALE;
                glBegin(GL_LINES);
                glVertex2i(tileX, tileY + 1);
                glVertex2i(tileX, tileY + MAP_SCALE - 1);
                glEnd();
            }
            else if (map[y*MAPX+ x] == 3) {
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

void movef(int speed, float move_direction, float *positionX, float *positionY, float modifier) {
    door_struct* door;
    door = NULL;
    float tmpX = *positionX + cos(move_direction) * modifier * delta_frames;
    float tmpY = *positionY + sin(move_direction) * modifier * delta_frames;
    int tmpYmap = map[((int)tmpY>>TILE_POW) * MAPX + ((int)(*positionX)>>TILE_POW)];
    int tmpXmap = map[((int)(*positionY)>>TILE_POW) * MAPX + ((int)tmpX>>TILE_POW)];
    int oldposY = *positionY;
    if (tmpYmap == 3 || tmpYmap == 2)
        door = getDoor(((int)(*positionX)>>TILE_POW), ((int)tmpY>>TILE_POW));
    if (tmpYmap == 0 || door != NULL && door->exte == 0.0)
        *positionY = tmpY;
    door = NULL;
    if (tmpXmap == 3 || tmpXmap == 2)
        door = getDoor(((int)tmpX>>TILE_POW), ((int)(oldposY)>>TILE_POW));
    if (tmpXmap == 0 || door != NULL && door->exte == 0.0)
        *positionX = tmpX;
}

void check_inputs() {
    if (newkeys.p)
        exit(0);
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
        playerAngle -= 0.005 * delta_frames;
        radian_change(&playerAngle);
    }
    if (newkeys.e) {
        playerAngle += 0.005 * delta_frames;
        radian_change(&playerAngle);
    }
    if (newkeys.m && !oldkeys.m)
        show_map -= 1;
    
    if (move_direction_h >= 0.0) {
        if (move_direction_v >= 0.0) {
            move_direction_h = (move_direction_v + move_direction_h) / 2;
            if (move_direction_v == 0.0 && move_direction_h == (float)(0.75 * PI))
                move_direction_h = 1.75 * PI;
        }
        move_direction_h += playerAngle;
        radian_change(&move_direction_h);
        movef(PLAYER_SPEED, move_direction_h, &playerX, &playerY, 0.2f);
    }
    else if (move_direction_v >= 0.0) {
        move_direction_v += playerAngle;
        radian_change(&move_direction_v);
        movef(PLAYER_SPEED, move_direction_v, &playerX, &playerY, 0.2f);
    }

    sprites[0].direction += 0.00125 * delta_frames + PI;
    radian_change(&sprites[0].direction);
    movef(PLAYER_SPEED / 4, sprites[0].direction, &sprites[0].posX, &sprites[0].posY, 0.2f);
    sprites[0].direction -= PI;
    radian_change(&sprites[0].direction);
    sprites[0].source->posX = sprites[0].posX;
    sprites[0].source->posY = sprites[0].posY;

    float rayXH, rayYH, distH, rayXV, rayYV, distV;
    float Sin = sin(playerAngle);
    float Cos = cos(playerAngle);
    float Tan = Sin / Cos;
    float invTan = Cos / Sin;
    int raymapX, raymapY;
    door_struct* door;
    if (newkeys.space) {
        //up/down
        if (Sin < -0.0001) {
            rayYH = ((((int)playerY)>>TILE_POW)<<TILE_POW) - 0.0001;
            rayXH = playerX - (playerY - rayYH) * invTan;
            distH = - (playerY - rayYH) / Sin;
        }
        else if (Sin > 0.0001) {
            rayYH = ((((int)playerY)>>TILE_POW)<<TILE_POW) + TILE;
            rayXH = playerX - (playerY - rayYH) * invTan;
            distH = - (playerY - rayYH) / Sin;
        }
        else {
            rayXH = playerX;
            rayYH = playerY;
            distH = 10000.0f;
        }

        //left/right
        if (Cos > 0.0001) {
            rayXV = ((((int)playerX)>>TILE_POW)<<TILE_POW) + TILE;
            rayYV = playerY - (playerX - rayXV) * Tan;
            distV = - (playerX - rayXV) / Cos;
        }
        else if (Cos < -0.0001) {
            rayXV = ((((int)playerX)>>TILE_POW)<<TILE_POW) - 0.001;
            rayYV = playerY - (playerX - rayXV) * Tan;
            distV = - (playerX - rayXV) / Cos;
        }
        else {
            rayXV = playerX;
            rayYV = playerY;
            distV = 10000.0f;
        }
        if (distV < distH) {
            rayXH = rayXV;
            rayYH = rayYV;
        }
        raymapX = (int)(rayXH)>>TILE_POW;
        raymapY = (int)(rayYH)>>TILE_POW;
        if (map[raymapX + raymapY * MAPX] == 2 || map[raymapX + raymapY * MAPX] == 3) {
            door = getDoor(raymapX, raymapY);
            door->exte_rate = -0.5;
        }
    }
    glutPostRedisplay();
}

void render() {
    int ray;
    line_out_info* render_infoP = &render_info[0];
    int start, end;
    int hposition;
    int tex_index;
    int bright_index;
    float floor_ray;
    float textureY;
    float deltatextureY;
    float offset;
    float r, g, b;
    float fogstrength;
    for (ray=0;ray<RES;ray++) {
        if (show_map) {
            glColor3f(0, 1, 0);
            glLineWidth(1);
            glBegin(GL_LINES);
            glVertex2i(playerX / TILE * MAP_SCALE, playerY / TILE * MAP_SCALE);
            glVertex2i(render_infoP->rayX / TILE * MAP_SCALE, render_infoP->rayY / TILE * MAP_SCALE);
            glEnd();
        }

        deltatextureY = 64.0 / render_infoP->l_height;
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
        int lightX, lightY;
        int lightXremain, lightYremain;
        float line_diff;
        float line_sum;

        lightX = (int)render_infoP->rayX>>LIGHT_POW;
        lightY = (int)render_infoP->rayY>>LIGHT_POW;
        lightXremain = lightX % (TILE / LIGHT_GRID);
        lightYremain = lightY % (TILE / LIGHT_GRID);
        line_diff = render_infoP->rayX - (lightX<<LIGHT_POW) - render_infoP->rayY + (lightY<<LIGHT_POW);
        line_sum = render_infoP->rayX - (lightX<<LIGHT_POW) + render_infoP->rayY - (lightY<<LIGHT_POW);

        //btm rght btm
        if (lightXremain == (TILE / LIGHT_GRID) - 1 && lightYremain == (TILE / LIGHT_GRID) - 1 && line_diff<0)
            bright_index = LIGHT_MAP_L + 4 * (lightY>>(TILE_POW - LIGHT_POW)) * MAPX + 4 * (lightX>>(TILE_POW - LIGHT_POW)) + 3;

        //btm lft btm
        else if (lightXremain == 0 && lightYremain == (TILE / LIGHT_GRID) - 1 && line_sum>LIGHT_GRID)
            bright_index = LIGHT_MAP_L + 4 * (lightY>>(TILE_POW - LIGHT_POW)) * MAPX + 4 * (lightX>>(TILE_POW - LIGHT_POW)) + 2;

        //tp rght tp
        else if (lightXremain == (TILE / LIGHT_GRID) - 1 && lightYremain == 0 && line_sum<LIGHT_GRID)
            bright_index = LIGHT_MAP_L + 4 * (lightY>>(TILE_POW - LIGHT_POW)) * MAPX + 4 * (lightX>>(TILE_POW - LIGHT_POW)) + 1;

        //tp lft tp
        else if (lightXremain == 0 && lightYremain == 0 && line_diff>0)
            bright_index = LIGHT_MAP_L + 4 * (lightY>>(TILE_POW - LIGHT_POW)) * MAPX + 4 * (lightX>>(TILE_POW - LIGHT_POW));

        else
            bright_index = lightY*MAPX*(TILE/LIGHT_GRID) + lightX;
        if (LIGHT_MAP_L + 4 * MAPX * MAPY <= bright_index || bright_index < 0)
            bright_index = 0;

        glPointSize(SCALE);
        for (hposition=start; hposition<end;hposition++) {
            tex_index= (int)((int)(textureY) * TEXWIDTH + render_infoP->textureX);
            if (render_infoP->isdoor) {
                r = texture_missing[tex_index][red];
                g = texture_missing[tex_index][green];
                b = texture_missing[tex_index][blue];
            }
            else {
                r = texture_one[tex_index][red];
                g = texture_one[tex_index][green];
                b = texture_one[tex_index][blue];
            }
            fogstrength = -TILE / 100.0 * FADES / DFADE + 1.0 / (float)(TILE) / DFADE * render_infoP->dist;
            if (fogstrength < 0)
        		fogstrength = 0;
        	else if (fogstrength > 1.0)
        		fogstrength = 1.0;
            
            r *= brightness[bright_index * CHANNELS + red];
            g *= brightness[bright_index * CHANNELS + green];
            b *= brightness[bright_index * CHANNELS + blue];
            r = r * (1.0 - fogstrength) + fog[red] * fogstrength;
            g = g * (1.0 - fogstrength) + fog[green] * fogstrength;
            b = b * (1.0 - fogstrength) + fog[blue] * fogstrength;

            glColor3f(r, g, b);
            glBegin(GL_POINTS);
            glVertex2i(ray * SCALE - SCALE, hposition);
            glEnd();
            textureY += deltatextureY;
        }
        for (hposition = 0;hposition<HEIGHT - end;hposition++) {
        	floor_ray = floor_const / (hposition + end - HEIGHT / 2.0) / render_infoP->correct_fish;
            fogstrength = -TILE / 100.0 * FADES / DFADE + 1.0 / (float)(TILE) / DFADE * floor_ray;
        	if (fogstrength < 0)
        		fogstrength = 0;
        	else if (fogstrength > 1.0)
        		fogstrength = 1.0;
            tex_index = (int)(floor_ray * render_infoP->Sin + playerY) % 64 * 64 + (int)(floor_ray * render_infoP->Cos + playerX) % 64;
            if (tex_index > 63 * 64)
                tex_index = 63*64;
            r = texture_missing[tex_index][red];
            g = texture_missing[tex_index][green];
            b = texture_missing[tex_index][blue];
            
            lightX = (int)(floor_ray * render_infoP->Cos + playerX)>>LIGHT_POW;
            lightY = (int)(floor_ray * render_infoP->Sin + playerY)>>LIGHT_POW;
            bright_index = lightY*MAPX*(TILE/LIGHT_GRID) + lightX;
            if (LIGHT_MAP_L + 4 * MAPX * MAPY <= bright_index || bright_index < 0)
                bright_index = 0;

            r *= brightness[bright_index * CHANNELS + red];
            g *= brightness[bright_index * CHANNELS + green];
            b *= brightness[bright_index * CHANNELS + blue];
            r = r * (1.0 - fogstrength) + fog[red] * fogstrength;
            g = g * (1.0 - fogstrength) + fog[green] * fogstrength;
            b = b * (1.0 - fogstrength) + fog[blue] * fogstrength;
            glColor3f(r, g, b);
            glBegin(GL_POINTS);
            glVertex2i(ray * SCALE-SCALE, hposition + end);
            glEnd();
            /*
            r = texture_missing[tex_index][red] / 2.0;
            g = texture_missing[tex_index][green] / 2.0;
            b = texture_missing[tex_index][blue];
            */
            r = g = b = 1.0;
            
            r *= brightness[bright_index * CHANNELS + red];
            g *= brightness[bright_index * CHANNELS + green];
            b *= brightness[bright_index * CHANNELS + blue];
            r = r * (1.0 - fogstrength) + fog[red] * fogstrength;
            g = g * (1.0 - fogstrength) + fog[green] * fogstrength;
            b = b * (1.0 - fogstrength) + fog[blue] * fogstrength;
            glColor3f(r, g, b);
            glBegin(GL_POINTS);
            glVertex2i(ray * SCALE-SCALE, start -hposition);
            glEnd();
        }
        render_infoP++;
    }
}

void DDA() {
    int dofH = DOF;
    int dofV = DOF;
    float deltaYV, deltaYH;
    float deltaXV, deltaXH;
    float rayXV, rayYV;
    float rayXH, rayYH;
    float deltadistH, deltadistV;
    int ray, i;
    for (i=0;i<MAPX*MAPY;i++)
        visible[i] = expandable[i] = false;
    visible[(((int)playerY)>>TILE_POW) * MAPX + (((int)playerX)>>TILE_POW)] = true;
    expandable[(((int)playerY)>>TILE_POW) * MAPX + (((int)playerX)>>TILE_POW)] = true;
    for (ray = 0; ray<RES;ray++) {
        float distH = 10000.0f;
        float distV = 10000.0f;
        float angle = playerAngle - SHIFT + angles[ray];
        radian_change(&angle);
        float correct_fish = playerAngle - angle;
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
            rayYH = ((((int)playerY)>>TILE_POW)<<TILE_POW) - 0.0001;
            rayXH = playerX - (playerY - rayYH) * invTan;
            distH = - (playerY - rayYH) / Sin;
            dofH = 0;
        }
        else if (Sin > 0.0001) {
            deltaYH = TILE;
            deltaXH = TILE * invTan;
            deltadistH = TILE / Sin;
            rayYH = ((((int)playerY)>>TILE_POW)<<TILE_POW) + TILE;
            rayXH = playerX - (playerY - rayYH) * invTan;
            distH = - (playerY - rayYH) / Sin;
            dofH = 0;
        }
        else {
            rayXH = playerX;
            rayYH = playerY;
            distH = 10000.0f;
            dofH = DOF;
        }

        //left/right
        if (Cos > 0.0001) {
            deltaXV = TILE;
            deltaYV = TILE * Tan;
            deltadistV = TILE / Cos;
            rayXV = ((((int)playerX)>>TILE_POW)<<TILE_POW) + TILE;
            rayYV = playerY - (playerX - rayXV) * Tan;
            distV = - (playerX - rayXV) / Cos;
            dofV = 0;
        }
        else if (Cos < -0.0001) {
            deltaXV = - TILE;
            deltaYV = - TILE * Tan;
            deltadistV = - TILE / Cos;
            rayXV = ((((int)playerX)>>TILE_POW)<<TILE_POW) - 0.001;
            rayYV = playerY - (playerX - rayXV) * Tan;
            distV = - (playerX - rayXV) / Cos;
            dofV = 0;
        }
        else {
            rayXV = playerX;
            rayYV = playerY;
            distV = 10000.0f;
            dofV = DOF;
        }
        int raymapXH, raymapYH, raymapH;
        int raymapXV, raymapYV, raymapV;
        door_struct* doorH;
        door_struct* doorV;
        doorH = NULL;
        doorV = NULL;
        while (dofV < DOF || dofH < DOF) {
            if (dofH < DOF && distH <= distV) {
                raymapXH = (int)(rayXH)>>TILE_POW;
                raymapYH = (int)(rayYH)>>TILE_POW;
                raymapH = raymapXH + raymapYH * MAPX;
                if (raymapH > 0 && raymapH < MAPX * MAPY && !visible[raymapH]) {
                    visible[raymapH] = expandable[raymapH]  = true;
                }
                if (raymapH > 0 && raymapH < MAPX * MAPY && map[raymapH] == 1) {
                    dofH = DOF;
                    doorH = NULL;
                    break;
                }
                else if (raymapH > 0 && raymapH < MAPX * MAPY && map[raymapH] == 3) {
                    doorH = getDoor(raymapXH, raymapYH);
                    if (doorH->x * TILE - doorH->exte + TILE < rayXH + 0.5 * deltaXH) {
                        dofH = DOF;
                        distH += 0.5 * deltadistH;
                        rayXH += 0.5 * deltaXH;
                        rayYH += 0.5 * deltaYH;
                        raymapXH = (int)(rayXH)>>TILE_POW;
                        raymapYH = (int)(rayYH)>>TILE_POW;
                        raymapH = raymapXH + raymapYH * MAPX;
                        break;
                    }
                    else {
                        rayXH += deltaXH;
                        rayYH += deltaYH;
                        distH += deltadistH;
                        dofH++;
                    }
                }
                else if (raymapH > 0 && raymapH < MAPX * MAPY && map[raymapH] == 4) {
                    dofH = DOF;
                    distH += 0.5 * deltadistH;
                    rayXH += 0.5 * deltaXH;
                    rayYH += 0.5 * deltaYH;
                    raymapXH = (int)(rayXH)>>TILE_POW;
                    raymapYH = (int)(rayYH)>>TILE_POW;
                    raymapH = raymapXH + raymapYH * MAPX;
                    doorH = NULL;
                    doorV = NULL;
                    break;
                }
                else {
                    rayXH += deltaXH;
                    rayYH += deltaYH;
                    distH += deltadistH;
                    dofH++;
                }
            }
            else if (dofV < DOF && distV < distH) {
                raymapXV = (int)(rayXV)>>TILE_POW;
                raymapYV = (int)(rayYV)>>TILE_POW;
                raymapV = raymapXV + raymapYV * MAPX;
                if (raymapV > 0 && raymapV < MAPX * MAPY && !visible[raymapV]) {
                    visible[raymapV] = expandable[raymapV] = true;
                }
                if (raymapV > 0 && raymapV < MAPX * MAPY && map[raymapV] == 1) {
                    dofV = DOF;
                    doorV = NULL;
                    break;
                }
                else if (raymapV > 0 && raymapV < MAPX * MAPY && map[raymapV] == 2) {
                    doorV = getDoor(raymapXV, raymapYV);
                    if (doorV->y * TILE - doorV->exte + TILE < rayYV + 0.5 * deltaYV) {
                        dofV = DOF;
                        distV += 0.5 * deltadistV;
                        rayXV += 0.5 * deltaXV;
                        rayYV += 0.5 * deltaYV;
                        raymapXV = (int)(rayXV)>>TILE_POW;
                        raymapYV = (int)(rayYV)>>TILE_POW;
                        raymapV = raymapXV + raymapYV * MAPX;
                        break;
                    }
                    else {
                        rayXV += deltaXV;
                        rayYV += deltaYV;
                        distV += deltadistV;
                        dofV++;
                    }
                }
                else if (raymapV > 0 && raymapV < MAPX * MAPY && map[raymapV] == 5) {
                    dofV = DOF;
                    distV += 0.5 * deltadistV;
                    rayXV += 0.5 * deltaXV;
                    rayYV += 0.5 * deltaYV;
                    raymapXV = (int)(rayXV)>>TILE_POW;
                    raymapYV = (int)(rayYV)>>TILE_POW;
                    raymapV = raymapXV + raymapYV * MAPX;
                    doorH = NULL;
                    doorV = NULL;
                    break;
                }
                else {
                    rayXV += deltaXV;
                    rayYV += deltaYV;
                    distV += deltadistV;
                    dofV++;
                }
            }
            else
                break;
        }

        /*
        int j;
        if (newkeys.l) {
            for (i=0;i<MAPY;i++) {
                for (j=0;j<MAPX;j++) {
                    printf("%b ", visible[i * MAPX + j]);
                }
                printf("\n");
            }
            printf("\n");
        }
        */



        float textureX;
        if (distV <= distH) {
            if (doorV == NULL) {
                render_info[ray].isdoor = false;
                if (Cos > 0)
                    render_info[ray].textureX = (int)rayYV % 64;
                else
                    render_info[ray].textureX = 63 - (int)rayYV % 64;
            }
            else {
                render_info[ray].textureX = -64 + (int)rayYV % 64 + ceil(doorV->exte);
                render_info[ray].isdoor = true;
            }
            render_info[ray].dist = distV;
            render_info[ray].l_height = (float)(TILE * HEIGHT) / distV / correct_fish;
            render_info[ray].rayY = rayYV;
            render_info[ray].rayX = rayXV;
        }
        else {
            if (doorH == NULL) {
                render_info[ray].isdoor = false;
                if (Sin < 0)
                    render_info[ray].textureX = (int)rayXH % 64;
                else
                    render_info[ray].textureX = 63 - (int)rayXH % 64;
            }
            else {
                render_info[ray].textureX = -64 + (int)rayXH % 64 + ceil(doorH->exte);
                render_info[ray].isdoor = true;
            }
            render_info[ray].dist = distH;
            render_info[ray].l_height = (float)(TILE * HEIGHT) / distH / correct_fish;
            render_info[ray].rayY = rayYH;
            render_info[ray].rayX = rayXH;
        }
        render_info[ray].correct_fish = correct_fish;
        render_info[ray].Cos = Cos;
        render_info[ray].Sin = Sin;
    }
}

void doorf() {
    door_struct* door;
    door = &doors[0];
    int doornum;
    for (doornum=0;doornum<DOORN;doornum++) {
        if (door->exte_rate < 0) {
            door->exte += door->exte_rate * delta_frames / 15;
            if (door->exte < 0) {
                door->exte_rate = 0.5;
                door->exte = 0.0;
                door->wait = 300;
            }
        }
        else if (door->wait > 0)
            door->wait -= 1 * delta_frames / 15;
        else if ((((int)playerX)>>6) != door->x || (((int)playerY)>>6) != door->y) {
            door->exte += door->exte_rate * delta_frames / 15;
            if (door->exte > 64) {
                door->exte_rate = 0.0;
                door->exte = 64.0;
            }
        }
        door++;
    }
}

void expand_visible() {
    int i, j, index;
    int limit = MAPX * MAPY;
    for (i=0;i<MAPY;i++) {
        for (j=0;j<MAPX;j++) {
            index = i * MAPX + j;
            if (visible[index] && expandable[index]) {
                if (index - 1 < limit && index - 1>0)
                    visible[index - 1] = true;
                if (index + 1 < limit && index + 1>0)
                    visible[index + 1] = true;
                if (index - MAPX < limit && index - MAPX>0)
                    visible[index - MAPX] = true;
                if (index + MAPX < limit && index + MAPX>0)
                    visible[index + MAPX] = true;
            }
        }
    }
}

void display() {
    current_frame = glutGet(GLUT_ELAPSED_TIME);
    delta_frames = current_frame - last_frame;
    last_frame = glutGet(GLUT_ELAPSED_TIME);
    check_inputs();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    doorf();
    gen_light(*sprites[0].source, light_maps);

    int i, color;
    for (color=0;color<CHANNELS;color++) {
        for (i=0;i< LIGHT_MAP_L + 4 * MAPX * MAPY;i++) {
            brightness[i * CHANNELS + color] = stationary[i * CHANNELS + color];
        }
    }

    combine_light(brightness, light_maps);
    DDA();
    expand_visible();
    render();
    if (show_map)
        drawMap();
    glutSwapBuffers();
    //combine_light(player_light, false);
    oldkeys = newkeys;
}

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
    if (key == 'l')
        newkeys.l = 1;
}

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
    if (key == 'l')
        newkeys.l = 0;
}

void initmap(FILE* fptr) {
    int num = 1;
    int mapXY[2];
    parsel(fptr, 2, mapXY, -1);
    //*mapX = mapXY[0];
    //*mapY = mapXY[1];
    MAPX = mapXY[0];
    MAPY = mapXY[1];
    LIGHT_MAP_L = MAPX * MAPY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID);
    FULL_LML = (LIGHT_MAP_L + MAPX * MAPY * 4) * CHANNELS;
    map = (int *)malloc(sizeof(int) * MAPX * MAPY);
    visible = calloc(MAPX * MAPY, sizeof(bool));
    expandable = calloc(MAPX * MAPY, sizeof(bool));
//bool visible[MAPX * MAPY] = { false };
//bool expandable[MAPX * MAPY] = { false };
    stationary = calloc(FULL_LML, sizeof(float));
    brightness = calloc(FULL_LML, sizeof(float));
    int mapline[mapXY[1]];
    int i, j;
    for (i=0;i<mapXY[1];i++) {
        parsel(fptr, mapXY[0], mapline,-1);
        for (j=0;j<mapXY[0];j++) {
            map[i*mapXY[0] + j] = mapline[j];
    		if (map[i * MAPX + j] == 2 || map[i * MAPX + j] == 3)
			    DOORN++;
        }
    }
    doors = malloc(sizeof(*doors) * DOORN);
    DOORN = 0;
    for (i=0;i<MAPY;i++) {
    	for (j=0;j<MAPX;j++) {
    		if (map[i * MAPX + j] == 2 || map[i * MAPX + j] == 3) {
    			doors[DOORN].x = j;
			    doors[DOORN].y = i;
			    doors[DOORN].exte = 64.0;
			    doors[DOORN].exte_rate = 0.0;
			    doors[DOORN].wait = 0;
			    DOORN++;
			}
		}
	}

    int playerXYA[3];
    parsel(fptr, 3, playerXYA, -1);
    playerX = playerXYA[0];
    playerY = playerXYA[1];
    playerAngle = playerXYA[2] / 50.0 * PI;
    intamb = (int *)malloc(sizeof(int) * 3);
    parsel(fptr, 3, intamb, 0);
    int fogse[2];
    parsel(fptr, 2, fogse, -1);
    DOF = (float)fogse[1];
    FADES = (float)fogse[0];
    DFADE = DOF - FADES;
    intfog = (int *)malloc(sizeof(int) * 3);
    parsel(fptr, 3, intfog, 0);
    for (i=0;i<CHANNELS;i++) {
        amb[i] = intamb[i] / 255.0;
        fog[i] = intfog[i] / 255.0;
    }
    int lightnum[1];
    parsel(fptr, 1, lightnum, -1);

    light_source stat_light;
    stat_light.is_static = 1;
    float init_light_map[(LIGHT_MAP_L + 4 * MAPX * MAPY) * CHANNELS];



    int lightslocal[lightnum[0]][6];
    for (i=0;i<lightnum[0];i++) {
        parsel(fptr, 6, lightslocal[i], 3);
        stat_light.posX = lightslocal[i][0];
        stat_light.posY = lightslocal[i][1];
        stat_light.intens = lightslocal[i][2];
        stat_light.r = lightslocal[i][3] / 255.0;
        stat_light.g = lightslocal[i][4] / 255.0;
        stat_light.b = lightslocal[i][5] / 255.0;
        gen_light(stat_light, init_light_map);
        combine_light(stationary, init_light_map);
    }
    for (i=0;i<LIGHT_MAP_L + MAPX * MAPY * 4 - 1;i++) {
        init_light_map[i * CHANNELS + red] = amb[red];
        init_light_map[i * CHANNELS + green] = amb[green];
        init_light_map[i * CHANNELS + blue] = amb[blue];
    }
    combine_light(stationary, init_light_map);

    int spr_num[1];
    parsel(fptr, 1, spr_num, -1);
    light_maps = calloc(FULL_LML * spr_num[0], sizeof(float));
    int spriteinfo[spr_num[0]][7];
    for (i=0;i<spr_num[0];i++) {
        parsel(fptr, 7, spriteinfo[i], 4);
        sprites[i].posX = spriteinfo[i][0];
        sprites[i].posY = spriteinfo[i][1];
        sprites[i].direction = spriteinfo[i][2];
        sprites[i].is_static = 0;
        sprites[i].source = &lights[i];
        sprites[i].source->posX = spriteinfo[i][0];
        sprites[i].source->posY = spriteinfo[i][1];
        sprites[i].source->intens = spriteinfo[i][3];
        sprites[i].source->is_static = 0;
        sprites[i].source->r = spriteinfo[i][4] / 255.0;
        sprites[i].source->g = spriteinfo[i][5] / 255.0;
        sprites[i].source->b = spriteinfo[i][6] / 255.0;
    }

    /*
    printf("%d %d\n", mapXY[0], mapXY[1]);
    for (i=0;i<mapXY[1];i++) {
        for (j=0;j<mapXY[0];j++)
            printf("%d ", map[i*mapXY[0] + j]);
        printf("\n");
    }
    printf("%d %d %d\n", intamb[0], intamb[1], intamb[2]);
    printf("%d %d\n", fogse[0], fogse[1]);
    printf("%d %d %d\n", intfog[0], intfog[1], intfog[2]);
    printf("%d\n", lightnum[0]);
    for (i=0;i<lightnum[0];i++)
        printf("%d %d %d %d %d %d\n", lightslocal[i][0], lightslocal[i][1], lightslocal[i][2], lightslocal[i][3], lightslocal[i][4], lightslocal[i][5]);
    printf("%d\n", spr_num[0]);
    for (i=0;i<spr_num[0];i++)
        printf("%d %d %d %d %d %d %d\n", spriteinfo[i][0], spriteinfo[i][1], spriteinfo[i][2], spriteinfo[i][3], spriteinfo[i][4], spriteinfo[i][5], spriteinfo[i][6]);
    */
}

void init() {
    FILE* fptr = fopen("map.map", "r");
    initmap(fptr);
    //exitmap();
    fptr = NULL;
    //light_source stat_light;
    //float init_light_map[(LIGHT_MAP_L + 4 * MAPX * MAPY) * CHANNELS];
    glClearColor(0.3,0.3,0.3,0);
    gluOrtho2D(0,(RES - 2)*SCALE,(HEIGHT - 2)*FLOOR_SCALE,0);

    /*
    stat_light.posX = 3.51 * TILE;
    stat_light.posY = 2.51 * TILE;
    stat_light.intens = 10000.0f;
    stat_light.is_static = 1;
    stat_light.r = 1;
    stat_light.g = 0;
    stat_light.b = 0;

    gen_light(stat_light, stationary);

    stat_light.posX = 5.5 * TILE;
    stat_light.posY = 4.5 * TILE;
    stat_light.r = 0;
    stat_light.g = 1;
    stat_light.b = 0;
    gen_light(stat_light, init_light_map);
    combine_light(stationary, init_light_map);

    stat_light.posX = 3.5 * TILE;
    stat_light.posY = 4.5 * TILE;
    stat_light.r = 0;
    stat_light.g = 0;
    stat_light.b = 1;
    gen_light(stat_light, init_light_map);
    combine_light(stationary, init_light_map);

    int i;
    for (i=0;i<LIGHT_MAP_L + MAPX * MAPY * 4 - 1;i++) {
        init_light_map[i * CHANNELS + red] = amb[red];
        init_light_map[i * CHANNELS + green] = amb[green];
        init_light_map[i * CHANNELS + blue] = amb[blue];
    }
    combine_light(stationary, init_light_map);

    sprites[0].posX = 3.5 * TILE;
    sprites[0].posY = 7.5 * TILE;
    sprites[0].direction = 0.0f;
    sprites[0].is_static = 0;
    sprites[0].source = &lights[0];
    sprites[0].source->posX = sprites[0].posX;
    sprites[0].source->posY = sprites[0].posY;
    sprites[0].source->intens = 10000.0f;
    sprites[0].source->is_static = 0;
    sprites[0].source->r = 1;
    sprites[0].source->g = 1;
    sprites[0].source->b = 1;
    */

    fptr = fopen("missing.ppm", "r");
    if (fptr != NULL) {
        if (parse(fptr, texture_missing))
            exit(1);
    }
    else {
        printf("Failed to open file missing.ppm\n");
        exit(1);
    }
    fptr = fopen("tile.ppm", "r");
    if (fptr != NULL) {
        if (parse(fptr, texture_one))
            exit(1);
    }
    else {
        printf("Failed to open file tile.ppm\n");
        exit(1);
    }
    float base_angle = 0.5 * PI - SHIFT;
    radian_change(&base_angle);
    float baseCos = cos(base_angle);
    float side = RES * sin(base_angle) / sin(FOV);
    int i;
    for (i=0; i<RES; i++)
        angles[i] = acos((side - i * baseCos) / sqrt(side * side + i * i - 2 * side * i * baseCos));
}

int main(int argc, char* argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize((RES - 2) * SCALE, (HEIGHT - 2) * FLOOR_SCALE);
    glutCreateWindow("Raycaster");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keydown);
    glutKeyboardUpFunc(keyup);
    glutMainLoop();
}
