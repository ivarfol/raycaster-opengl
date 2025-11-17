#include <stdio.h>
#include <GL/freeglut.h>
#include <math.h>
#include <stdbool.h>

#define PI 3.1415926535
#define PLAYER_SPEED 5
#define RES 256
#define SCALE 4
#define FOV 0.5 * PI
#define SHIFT (FOV / 2)

#define MAP_SCALE 16
#define LIGHT_GRID 2

#define LIGHT_POW 1
#define TILE_POW 6

#define TEXWIDTH 64
#define TEXHEIGHT TEXWIDTH
#define CHANNELS 3
#define TILE 64

#define PLHEIGHT 32

#define FLOOR_SCALE 1
#define HEIGHT 512

#define MIN_BRIGHTNESS 0.01

#define LDOF 10

#define LTEX_S (32 * (TILE / LIGHT_GRID))

#define SQR(a) ((a)*(a))

#define OPACITY 0.25

#define DOOR_WAIT 300

#define MS_PER_FRAME 16

float texture_one[TEXWIDTH * TEXHEIGHT][CHANNELS];
float texture_missing[TEXWIDTH * TEXHEIGHT][CHANNELS];

extern int parse(FILE* fptr, float texture[][CHANNELS]);
extern void parsel(FILE* fptr, int num, int out[], int color_pos);

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
    int x;
    int y;
} point;

typedef struct {
    float x;
    float y;
} fpoint;

extern void filled_tr(point interv[], bool shadow[], int x);

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
    int bright_index;
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
light_source *lights;

typedef struct {
    float posX;
    float posY;
    float direction;
    float deltadir;
    float deltamove;
    light_source* source;
} sprite_strct;
sprite_strct *sprites;

double ltexture[SQR(LTEX_S)];

float playerX, playerY, playerAngle;
bool show_map = true;
float move_direction_v, move_direction_h;
double floor_const = HEIGHT * RES / 2 / 4 / tan(0.5*PI -SHIFT) ; // aspect 1/2
float current_frame = 0.0;
float delta_frames, last_frame;

float angles[RES];

int *map, light_map_length;

int mapX, mapY, spritenum;
int doornum;

float amb[CHANNELS], fog[CHANNELS];
float *stationary, *brightness;

fpoint *vert;
int vnum;

bool *visible, *expandable;

float DOF, fade_start, delta_fade;


int intamb[3];
int intfog[3];

void exitmap(int num) {
    switch (num) {
    case 7:
        free(lights);
        lights = NULL;
    case 6:
        free(sprites);
        sprites = NULL;
    case 5:
        free(doors);
        doors = NULL;
    case 4:
        free(brightness);
        brightness = NULL;
    case 3:
        free(stationary);
        stationary = NULL;
    case 2:
        free(expandable);
        expandable = NULL;
    case 1:
        free(visible);
        visible = NULL;
    case 0:
        free(map);
        map = NULL;
    }
}

void finalexit() {
    exitmap(7);
}

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

point path_free(float posX, float posY, float angle, float targdist) {
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
    while (dofV < LDOF || dofH < LDOF) {
        if (dofH < LDOF && distH <= distV) {
            raymapXH = (int)(rayXH)>>TILE_POW;
            raymapYH = (int)(rayYH)>>TILE_POW;
            raymapH = raymapXH + raymapYH * mapX;
            if (raymapH > 0 && raymapH < mapX * mapY && map[raymapH] == 1) {
                dofH = LDOF;
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
            raymapV = raymapXV + raymapYV * mapX;
            if (raymapV > 0 && raymapV < mapX * mapY && map[raymapV] == 1) {
                dofV = LDOF;
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
    point out;
    if (distV < distH) {
        out.x = (rayXV)  / LIGHT_GRID;
        out.y = (rayYV)  / LIGHT_GRID;
        if (deltaYV > 0)
            out.y += 1;
        if (distV + TILE / LIGHT_GRID < targdist || out.y < 0 || out.y > mapY * TILE / LIGHT_GRID || out.x < 0 || out.x > mapX * TILE / LIGHT_GRID)
            out.x = -TILE;
    }
    else {
        out.x = (rayXH)  / LIGHT_GRID;
        out.y = (rayYH)  / LIGHT_GRID;
        if (deltaYH > 0)
            out.y += 1;
        if (deltaXH > 0)
            out.x += 1;
        else
            out.x -= 1;
        if (distH + TILE / LIGHT_GRID < targdist || out.y < 0 || out.y > mapY * TILE / LIGHT_GRID || out.x < 0 || out.x > mapX * TILE / LIGHT_GRID)
            out.x = -TILE;
    }
    return out;
}

int compare(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

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

void genmissing() {
    int i, j, tmpc;
    for (i=0;i<TEXWIDTH;i++) {
        for (j=0;j<TEXWIDTH;j++) {
            if ((i < TEXWIDTH / 2 && j < TEXWIDTH / 2) || (i >= TEXWIDTH / 2&& j >= TEXWIDTH / 2))
                tmpc = 1;
            else
                tmpc = 0;
            texture_missing[i * TEXWIDTH + j][red] = tmpc;
            texture_missing[i * TEXWIDTH + j][green] = 0;
            texture_missing[i * TEXWIDTH + j][blue] = tmpc;
        }
    }
}

void gen_light(light_source light, float light_map[]) {
    int startX, endX;
    int startY, endY;
    float max_dist = sqrt(light.intens / MIN_BRIGHTNESS);
    startX = (int)(light.posX- max_dist)>>LIGHT_POW;
    //printf("stxb %d\n", startX<<LIGHT_POW);
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
    float vangle[vnumaf][2];
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
    qsort(vangle, vnumaf, sizeof(vangle[0]), compare);
    point interv[3];
    interv[0].x = (int)(light.posX) >> LIGHT_POW;
    interv[0].y = (int)(light.posY) >> LIGHT_POW;

    i = 0;

    interv[1] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1]);
    while (interv[1].x == -TILE) {
        i++;
        if (i >= vnumaf)
            break;
        interv[1] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1]);
    }
    point startp;
    startp = interv[1];

    bool lastisone = false;
    for (;i<vnumaf;i++) { //if target dist > actual dist + 64 ignore
        interv[2] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1]);
        while (interv[2].x == -TILE || (interv[2].x == interv[1].x && interv[2].y == interv[1].y)) {
            i++;
            if (i >= vnumaf)
                break;
            interv[2] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1]);
        }
        if (i >= vnumaf)
            break;
        lastisone = true;
        filled_tr(interv, shadow, mapX*(TILE/LIGHT_GRID));
        do {
            i++;
            if (i >= vnumaf)
                break;
            interv[1] = path_free(light.posX, light.posY, vangle[i][0], vangle[i][1]);
        } while (interv[1].x == -TILE || (interv[2].x == interv[1].x && interv[2].y == interv[1].y));
        if (i >= vnumaf)
            break;
        filled_tr(interv, shadow, mapX*(TILE/LIGHT_GRID));
        lastisone = false;
    }
    //printf("vnumaf %d\nlastisone %d\n", vnumaf, lastisone);
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
            if (visible[(tileY>>(TILE_POW-LIGHT_POW))*mapX + (tileX>>(TILE_POW-LIGHT_POW))] || light.is_static == 1) {
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
    /*
    if (newkeys.l) {
        for (tileY = 0; tileY < mapY * (TILE / LIGHT_GRID);tileY++) {
            for (tileX = 0; tileX < mapX * (TILE / LIGHT_GRID);tileX++) {
                printf("%0.2f ", light.light_map[tileY  * CHANNELS + mapY * (TILE / LIGHT_GRID) + tileX]);
            }
            printf("\n");
        }
        printf("\n");
    }
    */
}

void combine_light(float light_one[], float light_two[]) {
    int i, color;
    for (i=0;i<light_map_length;i++) {
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
            else if (map[y*mapX + x] == 2) {
                tileX = (x + 0.5) * MAP_SCALE;
                tileY = y * MAP_SCALE;
                glBegin(GL_LINES);
                glVertex2i(tileX, tileY + 1);
                glVertex2i(tileX, tileY + MAP_SCALE - 1);
                glEnd();
            }
            else if (map[y*mapX+ x] == 12) {
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
    int tmpYmap = map[((int)tmpY>>TILE_POW) * mapX + ((int)(*positionX)>>TILE_POW)];
    int tmpXmap = map[((int)(*positionY)>>TILE_POW) * mapX + ((int)tmpX>>TILE_POW)];
    int oldposY = *positionY;
    if (tmpYmap == 12 || tmpYmap == 2)
        door = getDoor(((int)(*positionX)>>TILE_POW), ((int)tmpY>>TILE_POW));
    if (tmpYmap == 0 || door != NULL && door->exte == 0.0)
        *positionY = tmpY;
    door = NULL;
    if (tmpXmap == 12 || tmpXmap == 2)
        door = getDoor(((int)tmpX>>TILE_POW), ((int)(oldposY)>>TILE_POW));
    if (tmpXmap == 0 || door != NULL && door->exte == 0.0)
        *positionX = tmpX;
}

void check_inputs() {
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

    

    float rayXH, rayYH, distH, rayXV, rayYV, distV;
    float Sin = sin(playerAngle);
    float Cos = cos(playerAngle);
    float Tan = Sin / Cos;
    float invTan = Cos / Sin;
    int raymapX, raymapY, i;
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
        if (map[raymapX + raymapY * mapX] == 2 || map[raymapX + raymapY * mapX] == 12) {
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
    int lightX, lightY;
    int i;
    for (ray=0;ray<RES;ray++) {
        if (show_map) {
            glColor3f(0, 1, 0);
            glLineWidth(1);
            glBegin(GL_LINES);
            glVertex2i(playerX / TILE * MAP_SCALE, playerY / TILE * MAP_SCALE);
            glVertex2i(render_infoP->rayX / TILE * MAP_SCALE, render_infoP->rayY / TILE * MAP_SCALE);
            glEnd();
        }
        /*
        for (i=0;i<vnum;i++) {
            glColor3f(0, 1, 0);
            glLineWidth(1);
            glBegin(GL_LINES);
            glVertex2i(224.0 / TILE * MAP_SCALE, 161.0 / TILE * MAP_SCALE);
            glVertex2i((float)(vert[i].x) / TILE * MAP_SCALE, (float)(vert[i].y) / TILE * MAP_SCALE);
            glEnd();
        }
        */

        deltatextureY = TEXHEIGHT / render_infoP->l_height;
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
            fogstrength = -TILE / 100.0 * fade_start / delta_fade + 1.0 / (float)(TILE) / delta_fade * render_infoP->dist;
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
            glVertex2i(ray * SCALE - SCALE, hposition);
            glEnd();
            textureY += deltatextureY;
        }
        for (hposition = 0;hposition<HEIGHT - end;hposition++) {
        	floor_ray = floor_const / (hposition + end - HEIGHT / 2.0) / render_infoP->correct_fish;
            fogstrength = -TILE / 100.0 * fade_start / delta_fade + 1.0 / (float)(TILE) / delta_fade * floor_ray;
        	if (fogstrength < 0)
        		fogstrength = 0;
        	else if (fogstrength > 1.0)
        		fogstrength = 1.0;
            tex_index = (int)(floor_ray * render_infoP->Sin + playerY) % TEXHEIGHT * TEXWIDTH + (int)(floor_ray * render_infoP->Cos + playerX) % TEXWIDTH;
            if (tex_index >= TEXWIDTH * TEXHEIGHT)
                tex_index = TEXWIDTH * TEXHEIGHT-1;
            r = texture_missing[tex_index][red];
            g = texture_missing[tex_index][green];
            b = texture_missing[tex_index][blue];
            
            lightX = (int)(floor_ray * render_infoP->Cos + playerX)>>LIGHT_POW;
            lightY = (int)(floor_ray * render_infoP->Sin + playerY)>>LIGHT_POW;
            bright_index = lightY*mapX*(TILE/LIGHT_GRID) + lightX;
            if (light_map_length * CHANNELS <= bright_index || bright_index < 0)
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
    bool is_fulltile = false;
    for (i=0;i<mapX*mapY;i++)
        visible[i] = expandable[i] = false;
    visible[(((int)playerY)>>TILE_POW) * mapX + (((int)playerX)>>TILE_POW)] = true;
    expandable[(((int)playerY)>>TILE_POW) * mapX + (((int)playerX)>>TILE_POW)] = true;
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
            rayXV = ((((int)playerX)>>TILE_POW)<<TILE_POW) - 0.0001;
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
        bool Hpropagate, Vpropagate;
        door_struct* doorH;
        door_struct* doorV;
        doorH = NULL;
        doorV = NULL;
        while (dofV < DOF || dofH < DOF) {
            if (dofH < DOF && distH <= distV) {
                Hpropagate = false;
                raymapXH = (int)(rayXH)>>TILE_POW;
                raymapYH = (int)(rayYH)>>TILE_POW;
                raymapH = raymapXH + raymapYH * mapX;
                if (raymapH > 0 && raymapH < mapX * mapY) {
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
                if (raymapV > 0 && raymapV < mapX * mapY) {
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

        /*
        int j;
        if (newkeys.l) {
            for (i=0;i<mapY;i++) {
                for (j=0;j<mapX;j++) {
                    printf("%b ", visible[i * mapX + j]);
                }
                printf("\n");
            }
            printf("\n");
        }
        */



        int br_offset = 0;
        if (distV <= distH) {
            if (doorV == NULL) {
                render_info[ray].isdoor = false;
                if (Cos > 0) {
                    render_info[ray].textureX = (int)rayYV % TEXWIDTH;
                    if (is_fulltile) {
                        if (render_info[ray].textureX>= TILE - LIGHT_GRID)
                            br_offset = -1*mapX*(TILE/LIGHT_GRID);
                        else if (render_info[ray].textureX<=LIGHT_GRID)
                            br_offset = 1*mapX*(TILE/LIGHT_GRID);
                    }
                }
                else {
                    render_info[ray].textureX = TEXWIDTH - 1 - (int)rayYV % TEXWIDTH;
                    if (is_fulltile) {
                        if (render_info[ray].textureX>= TILE - LIGHT_GRID)
                            br_offset = 1*mapX*(TILE/LIGHT_GRID);
                        else if (render_info[ray].textureX<=LIGHT_GRID)
                            br_offset = -1*mapX*(TILE/LIGHT_GRID);
                    }
                }
            }
            else {
                render_info[ray].textureX = -TEXWIDTH + (int)rayYV % TEXWIDTH + ceil(doorV->exte);
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
                if (Sin < 0) {
                    render_info[ray].textureX = (int)rayXH % TEXWIDTH;
                    if (is_fulltile) {
                        if (render_info[ray].textureX>= TILE - LIGHT_GRID)
                            br_offset = -1;
                        else if (render_info[ray].textureX<=LIGHT_GRID)
                            br_offset = 1;
                    }
                }
                else {
                    render_info[ray].textureX = TEXWIDTH - 1 - (int)rayXH % TEXWIDTH;
                    if (is_fulltile) {
                        if (render_info[ray].textureX>= TILE - LIGHT_GRID)
                            br_offset = 1;
                        else if (render_info[ray].textureX<=LIGHT_GRID)
                            br_offset = -1;
                    }
                }
            }
            else {
                render_info[ray].textureX = -TEXWIDTH + (int)rayXH % TEXWIDTH + ceil(doorH->exte);
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

        render_info[ray].bright_index = ((int)render_info[ray].rayY>>LIGHT_POW)*mapX*(TILE/LIGHT_GRID) + ((int)render_info[ray].rayX>>LIGHT_POW) + br_offset;
        if (light_map_length * CHANNELS <= render_info[ray].bright_index || render_info[ray].bright_index < 0)
            render_info[ray].bright_index = 0;
    }
}

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
        }
        else if (door->wait > 0)
            door->wait -= 1 * delta_frames / 15;
        else if ((((int)playerX)>>TILE_POW) != door->x || (((int)playerY)>>TILE_POW) != door->y) {
            door->exte += door->exte_rate * delta_frames / MS_PER_FRAME;
            if (door->exte > TILE) {
                door->exte_rate = 0.0;
                door->exte = TILE;
            }
        }
        door++;
    }
}

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
        }
    }

    float dlight_map[light_map_length * CHANNELS];
    for (i=0;i<spritenum;i++) {
        gen_light(*sprites[i].source, dlight_map);
        combine_light(brightness, dlight_map);
        sprites[i].direction += sprites[i].deltadir * delta_frames + PI;
        radian_change(&sprites[i].direction);
        movef(PLAYER_SPEED / 4.0, sprites[i].direction, &sprites[i].posX, &sprites[i].posY, sprites[i].deltamove);
        sprites[i].direction -= PI;
        radian_change(&sprites[i].direction);
        sprites[i].source->posX = sprites[i].posX;
        sprites[i].source->posY = sprites[i].posY;
    }

    DDA();
    expand_visible();
    render();
    if (show_map)
        drawMap();
    glutSwapBuffers();
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
    int mapXY[2];
    parsel(fptr, 2, mapXY, -1);
    mapX = mapXY[0];
    mapY = mapXY[1];
    light_map_length = mapX * mapY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID);
    map = malloc(sizeof(int) * mapX * mapY);
    if (map == NULL) {
        exitmap(0);
        exit(0);
    }
    visible = calloc(mapX * mapY, sizeof(bool));
    if (visible == NULL) {
        exitmap(1);
        exit(1);
    }
    expandable = calloc(mapX * mapY, sizeof(bool));
    if (expandable == NULL) {
        exitmap(2);
        exit(1);
    }
    stationary = calloc(light_map_length * CHANNELS, sizeof(float));
    if (stationary == NULL) {
        exitmap(3);
        exit(1);
    }
    brightness = calloc(light_map_length * CHANNELS, sizeof(float));
    if (brightness == NULL) {
        exitmap(4);
        exit(1);
    }
    int mapline[mapXY[1]];
    int i, j, ia, ja, neib_num;
    bool neib_b[4];
    vnum = 0;
    for (i=0;i<mapXY[1];i++) {
        parsel(fptr, mapXY[0], mapline,-1);
        for (j=0;j<mapXY[0];j++) {
            map[i*mapXY[0] + j] = mapline[j];
    		if ((i-1) * mapX + j - 1 >= 0) {
                neib_num = 0;
                for (ia=0;ia<2;ia++) {
                    for (ja=0;ja<2;ja++) {
                        if (map[(i+ia - 1) * mapX + j +ja - 1] == 1) {
                            neib_num++;
                        }
                    }
                }
                if (neib_num != 4 && neib_num != 0)
                    vnum++;
                if (neib_num == 1 || neib_num == 3)
                    vnum++;
            }
    		if (map[i * mapX + j] == 2)
			    doornum++;
        }
    }
    vert = malloc((vnum + 4) * sizeof(fpoint));
    doors = malloc(sizeof(*doors) * doornum);
    if (doors == NULL) {
        exitmap(5);
        exit(1);
    }
    doornum = 0;
    float vshiftX, vshiftY;
    vnum = 0;
    for (i=0;i<mapY;i++) {
    	for (j=0;j<mapX;j++) {
    		if (map[i * mapX + j] == 2) {
                if (map[i * mapX + j - 1] == 1 && map[i * mapX + j + 1] == 1)
                    map[i * mapX + j] = 12;
    			doors[doornum].x = j;
			    doors[doornum].y = i;
			    doors[doornum].exte = TILE;
			    doors[doornum].exte_rate = 0.0;
			    doors[doornum].wait = 0;
			    doornum++;
            }
    		else if (map[i * mapX + j] == 3) {
                if (map[i * mapX + j - 1] == 1 && map[i * mapX + j + 1] == 1)
                    map[i * mapX + j] = 13;
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
                vshiftX = 0.25 * (neib_b[0] + neib_b[1] - neib_b[2] - neib_b[3]);
                vshiftY = 0.25 * (neib_b[0] + neib_b[2] - neib_b[1] - neib_b[3]);
                if (neib_num != 4 && neib_num != 0) {
                    vert[vnum].x = j * TILE + vshiftX;
                    vert[vnum++].y = i * TILE + vshiftY;
                }
                if (neib_num == 1 || neib_num == 3) {
                    vert[vnum].x = j * TILE - vshiftX;
                    vert[vnum++].y = i * TILE - vshiftY;
                }
            }
		}
	}
    vnum+=4;

    int playerXYA[3];
    parsel(fptr, 3, playerXYA, -1);
    playerX = playerXYA[0];
    playerY = playerXYA[1];
    playerAngle = playerXYA[2] / 50.0 * PI;
    parsel(fptr, 3, intamb, 0);
    int fogse[2];
    parsel(fptr, 2, fogse, -1);
    DOF = (float)fogse[1];
    fade_start = (float)fogse[0];
    delta_fade = DOF - fade_start;
    parsel(fptr, 3, intfog, 0);
    for (i=0;i<CHANNELS;i++) {
        amb[i] = intamb[i] / 255.0;
        fog[i] = intfog[i] / 255.0;
    }
    int lightnum[1];
    parsel(fptr, 1, lightnum, -1);

    light_source stat_light;
    stat_light.is_static = 1;
    float init_light_map[light_map_length * CHANNELS];

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
    for (i=0;i<light_map_length;i++) {
        init_light_map[i * CHANNELS + red] = amb[red];
        init_light_map[i * CHANNELS + green] = amb[green];
        init_light_map[i * CHANNELS + blue] = amb[blue];
    }
    combine_light(stationary, init_light_map);

    int spr_num[1];
    parsel(fptr, 1, spr_num, -1);
    spritenum = spr_num[0];

    sprites = malloc(sizeof(*sprites) * spritenum);
    if (sprites == NULL) {
        exitmap(6);
        exit(1);
    }
    lights = malloc(sizeof(*lights) * spritenum);
    if (lights == NULL) {
        exitmap(7);
        exit(1);
    }
    int spriteinfo[spritenum][9];
    for (i=0;i<spritenum;i++) {
        parsel(fptr, 9, spriteinfo[i], 6);
        sprites[i].posX = spriteinfo[i][0];
        sprites[i].posY = spriteinfo[i][1];
        sprites[i].direction = spriteinfo[i][2] / 50.0 * PI;
        sprites[i].deltadir = spriteinfo[i][3] / 50000.0 * PI;
        sprites[i].deltamove = spriteinfo[i][4] / 100.0;
        sprites[i].source = &lights[i];
        sprites[i].source->posX = spriteinfo[i][0];
        sprites[i].source->posY = spriteinfo[i][1];
        sprites[i].source->intens = spriteinfo[i][5];
        sprites[i].source->r = spriteinfo[i][6] / 255.0;
        sprites[i].source->g = spriteinfo[i][7] / 255.0;
        sprites[i].source->b = spriteinfo[i][8] / 255.0;
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
    if (fptr == NULL) {
        printf("No map file found");
        exit(1);
    }
    genltex(LTEX_S, ltexture);
    initmap(fptr);
    fptr = NULL;
    glClearColor(0.3,0.3,0.3,0);
    gluOrtho2D(0,(RES - 2)*SCALE,(HEIGHT - 2)*FLOOR_SCALE,0);

    genmissing();

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
    glutCloseFunc(finalexit);
    glutDisplayFunc(display);
    glutKeyboardFunc(keydown);
    glutKeyboardUpFunc(keyup);
    glutMainLoop();
}
