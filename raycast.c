#include <stdio.h>
#include <GL/glut.h>
#include <math.h>
#include <stdbool.h>

#define PI 3.1415926535
#define PLAYER_SPEED 5
#define DOF 18
#define RES 256
#define SCALE 4
#define FOV 0.5 * PI
#define SHIFT FOV / 2
#define DOORN 2
#define MAP_SCALE 16
#define LIGHT_GRID 1

#define LIGHT_POW 0
#define TILE_POW 6

#define MIN_LIGHT_DIST 128

#define TEXWIDTH 64
#define TEXHEIGHT 64
#define CHANNELS 3
#define TILE 64

#define PLHEIGHT 32

#define HEIGHT 512
#define MAPX 8
#define MAPY 8

#define MIN_BRIGHTNESS 0.01

#define LIGHT_POS (4 * TILE) //light source

float texture_one[TEXWIDTH * TEXHEIGHT][CHANNELS];
float texture_missing[TEXWIDTH * TEXHEIGHT][CHANNELS];

extern int parse(FILE* fptr, float texture[][CHANNELS]);
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
door_struct doors[DOORN];

float playerX, playerY, playerAngle;
bool show_map = true;
float move_direction_v, move_direction_h;
double floor_const = PLHEIGHT * tan(SHIFT) * RES / 2 * SCALE; // aspect 1/2
float current_frame = 0.0;
float delta_frames, last_frame;

float angles[RES];

int map[MAPX * MAPY] =
{
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 1, 0, 0, 2, 0, 1,
    1, 3, 1, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
};
bool visible[MAPX * MAPY] = { false };

float brightness[MAPX * MAPY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID)][CHANNELS];
float combined[MAPX * MAPY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID)][CHANNELS];
//float player_light[MAPX * MAPY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID)][CHANNELS];
float lamp[MAPX * MAPY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID)][CHANNELS];

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

bool path_free(float posX, float posY, int tileX, int tileY, double angle, double dist) {
    float r, g, b;
    int dofH = DOF;
    int dofV = DOF;
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
    if ((((int)posX)>>TILE_POW) == tileX && (((int)posY)>>TILE_POW) == tileY)
        return true;

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
        dofH = DOF;
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
            if ((distH + sqrt(LIGHT_GRID * LIGHT_GRID * 2)>= dist && distH != 10000.0f))
                return true;
            raymapH = raymapXH + raymapYH * MAPX;
            if (raymapH > 0 && raymapH < MAPX * MAPY && map[raymapH] == 1) {
                dofH = DOF;
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
        else if (dofV < DOF && distV < distH) {
            raymapXV = (int)(rayXV)>>TILE_POW;
            raymapYV = (int)(rayYV)>>TILE_POW;
            if ((distV + sqrt(LIGHT_GRID * LIGHT_GRID * 2) >= dist && distV != 10000.0f))
                return true;
            raymapV = raymapXV + raymapYV * MAPX;
            if (raymapV > 0 && raymapV < MAPX * MAPY && map[raymapV] == 1) {
                dofV = DOF;
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

void gen_light(int lightX, int lightY, float intens, float bright_arr[][CHANNELS], float r, float g, float b, bool is_static) {
    int startX, endX;
    int startY, endY;
    float max_dist = sqrt(intens / MIN_BRIGHTNESS);
    startX = (int)(lightX - max_dist)>>LIGHT_POW;
    if (startX < 0)
        startX = 0;
    endX = (int)(lightX + max_dist)>>LIGHT_POW;
    if (endX > MAPX * (TILE / LIGHT_GRID))
        endX = MAPX * (TILE / LIGHT_GRID);

    startY = (int)(lightY - max_dist)>>LIGHT_POW;
    if (startY < 0)
        startY = 0;
    endY = (int)(lightY + max_dist)>>LIGHT_POW;
    if (endY > MAPY * (TILE / LIGHT_GRID))
        endY = MAPY * (TILE / LIGHT_GRID);

    int tileY, tileX;
    float distX, distY;
    float light;
    int i, color;
    for (color=0;color<CHANNELS;color++) {
        for (i = 0; i< MAPX * MAPY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID);i++)
            bright_arr[i][color] = 0;
    }
    double angle, dist;
    float mult;
    for (tileY = startY;tileY<endY;tileY++) {
        for (tileX = startX;tileX<endX;tileX++) {
            if (visible[(tileY>>(TILE_POW-LIGHT_POW))*MAPX + (tileX>>(TILE_POW-LIGHT_POW))] || is_static) {
                distX = tileX * LIGHT_GRID - lightX;
                distY = tileY * LIGHT_GRID - lightY;
                dist = sqrt(distX * distX + distY * distY);
                angle = atan(distY / distX);
                if (distX < 0)
                    angle += PI;
                mult = 0.25;
                if (!is_static || path_free(lightX, lightY, tileX>>(TILE_POW - LIGHT_POW), tileY>>(TILE_POW - LIGHT_POW), angle, dist))
                    mult = 1;
                light = intens / (distX * distX + distY * distY) * mult;
                if (light > 1)
                    light = 1;
                bright_arr[(tileY) * MAPY * (TILE / LIGHT_GRID) + tileX][red] = light * r;
                bright_arr[(tileY) * MAPY * (TILE / LIGHT_GRID) + tileX][green] = light * g;
                bright_arr[(tileY) * MAPY * (TILE / LIGHT_GRID) + tileX][blue] = light * b;
            }
        }
    }
    /*
    if (newkeys.l) {
        for (tileY = 0; tileY < MAPY * (TILE / LIGHT_GRID);tileY++) {
            for (tileX = 0; tileX < MAPX * (TILE / LIGHT_GRID);tileX++) {
                printf("%0.2f ", bright_arr[tileY * MAPY * (TILE / LIGHT_GRID) + tileX]);
            }
            printf("\n");
        }
        printf("\n");
    }
    */
}

void combine_light(float light[][CHANNELS], bool add) {
    int i, color;
    for (color=0;color<CHANNELS;color++) {
        for (i=0;i<MAPX * MAPY * (TILE / LIGHT_GRID) * (TILE / LIGHT_GRID);i++) {
            if (add) {
                combined[i][color] += light[i][color];
                if (combined[i][color] > 1)
                    brightness[i][color] = 1;
                else 
                    brightness[i][color] = combined[i][color];
            }
            else {
                combined[i][color] -= light[i][color];
                if (combined[i][color] < 0)
                    brightness[i][color] = 0;
                else 
                    brightness[i][color] = combined[i][color];
            }
            //printf("%0.2f ", brightness[i]);
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

void DDA() {
    float r, g, b;
    int dofH = DOF;
    int dofV = DOF;
    float deltaYV, deltaYH;
    float deltaXV, deltaXH;
    float rayXV, rayYV;
    float rayXH, rayYH;
    float deltadistH, deltadistV;
    int ray, i;
    for (i=0;i<MAPX*MAPY;i++)
        visible[i] = false;
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
        visible[(((int)playerY)>>TILE_POW) * MAPX + (((int)playerX)>>TILE_POW)] = true;
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
                if (raymapH > 0 && raymapH < MAPX * MAPY && map[raymapH] == 1) {
                    if (!visible[raymapH])
                        visible[raymapH] = true;
                    dofH = DOF;
                    doorH = NULL;
                    break;
                }
                else if (raymapH > 0 && raymapH < MAPX * MAPY && map[raymapH] == 3) {
                    if (!visible[raymapH])
                        visible[raymapH] = true;
                    doorH = getDoor(raymapXH, raymapYH);
                    if (doorH->x * TILE - doorH->exte + TILE < rayXH + 0.5 * deltaXH) {
                        dofH = DOF;
                        distH += 0.5 * deltadistH;
                        rayXH += 0.5 * deltaXH;
                        rayYH += 0.5 * deltaYH;
                        raymapXH = (int)(rayXH)>>TILE_POW;
                        raymapYH = (int)(rayYH)>>TILE_POW;
                        raymapH = raymapXH + raymapYH * MAPX;
                        if (raymapH > 0 && raymapH < MAPX * MAPY && !visible[raymapH])
                            visible[raymapH] = true;
                        break;
                    }
                    else {
                        rayXH += deltaXH;
                        rayYH += deltaYH;
                        distH += deltadistH;
                        dofH++;
                    }
                }
                else {
                    if (raymapH > 0 && raymapH < MAPX * MAPY && !visible[raymapH])
                        visible[raymapH] = true;
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
                if (raymapV > 0 && raymapV < MAPX * MAPY && map[raymapV] == 1) {
                    if (!visible[raymapV])
                        visible[raymapV] = true;
                    dofV = DOF;
                    doorV = NULL;
                    break;
                }
                else if (raymapV > 0 && raymapV < MAPX * MAPY && map[raymapV] == 2) {
                    if (!visible[raymapV])
                        visible[raymapV] = true;
                    doorV = getDoor(raymapXV, raymapYV);
                    if (doorV->y * TILE - doorV->exte + TILE < rayYV + 0.5 * deltaYV) {
                        dofV = DOF;
                        distV += 0.5 * deltadistV;
                        rayXV += 0.5 * deltaXV;
                        rayYV += 0.5 * deltaYV;
                        raymapXV = (int)(rayXV)>>TILE_POW;
                        raymapYV = (int)(rayYV)>>TILE_POW;
                        raymapV = raymapXV + raymapYV * MAPX;
                        if (raymapV > 0 && raymapV < MAPX * MAPY && !visible[raymapV])
                            visible[raymapV] = true;
                        break;
                    }
                    else {
                        rayXV += deltaXV;
                        rayYV += deltaYV;
                        distV += deltadistV;
                        dofV++;
                    }
                }
                else {
                    if (raymapV > 0 && raymapV < MAPX * MAPY && !visible[raymapV])
                        visible[raymapV] = true;
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

        if (show_map) {
            drawMap();
            if (distV <= distH) {
                glColor3f(0, 1, 0);
                glLineWidth(1);
                glBegin(GL_LINES);
                glVertex2i(playerX / TILE * MAP_SCALE, playerY / TILE * MAP_SCALE);
                glVertex2i(rayXV / TILE * MAP_SCALE, rayYV / TILE * MAP_SCALE);
                glEnd();
            }
            else {
                glColor3f(0, 0, 1);
                glLineWidth(1);
                glBegin(GL_LINES);
                glVertex2i(playerX / TILE * MAP_SCALE, playerY / TILE * MAP_SCALE);
                glVertex2i(rayXH / TILE * MAP_SCALE, rayYH / TILE * MAP_SCALE);
                glEnd();
            }
        }


        float textureX;
        if (distV <= distH) {
            distH = distV;
            if (doorV == NULL) {
                if (Cos > 0)
                    textureX = (int)rayYV % 64;
                else
                    textureX = 63 - (int)rayYV % 64;
            }
            else
                textureX = -64 + (int)rayYV % 64 + ceil(doorV->exte);
            doorH = doorV;
            rayYH = rayYV;
            rayXH = rayXV;
        }
        else {
            if (doorH == NULL) {
                if (Sin < 0)
                    textureX = (int)rayXH % 64;
                else
                    textureX = 63 - (int)rayXH % 64;
            }
            else
                textureX = -64 + (int)rayXH % 64 + ceil(doorH->exte);
        }

        float light_dist = distH;
        distH *= correct_fish;
        float line_height = (64.0 * HEIGHT) / distH;
        int start, end;
        float deltatextureY = 64.0 / line_height;
        float offset = 0.0;
        if (line_height > HEIGHT) {
            start = 0;
            end = HEIGHT;
            offset = (line_height - HEIGHT) / 2.0;
        }
        else {
            start = HEIGHT / 2.0 - 0.5 * line_height;
            end = HEIGHT / 2.0 + 0.5 * line_height;
        }
        int hposition;
        float textureY = offset * deltatextureY;
        int tex_index;
        for (hposition=start; hposition<end;hposition++) {
            tex_index= (int)((int)(textureY) * TEXWIDTH + textureX);
            if (doorH == NULL) {
                r = texture_one[tex_index][red];
                g = texture_one[tex_index][green];
                b = texture_one[tex_index][blue];
            }
            else {
                r = texture_missing[tex_index][red];
                g = texture_missing[tex_index][green];
                b = texture_missing[tex_index][blue];
            }
            r *= brightness[((int)rayYH>>LIGHT_POW)*MAPY*(TILE/LIGHT_GRID) + ((int)rayXH>>LIGHT_POW)][red];
            g *= brightness[((int)rayYH>>LIGHT_POW)*MAPY*(TILE/LIGHT_GRID) + ((int)rayXH>>LIGHT_POW)][green];
            b *= brightness[((int)rayYH>>LIGHT_POW)*MAPY*(TILE/LIGHT_GRID) + ((int)rayXH>>LIGHT_POW)][blue];
            glColor3f(r, g, b);
            glPointSize(SCALE);
            glBegin(GL_POINTS);
            glVertex2i(ray * SCALE, hposition);
            glEnd();
            textureY += deltatextureY;
        }
        float floor_ray;
        for (hposition = 0;hposition<HEIGHT - end;hposition++) {
            floor_ray = floor_const / (hposition + end - HEIGHT / 2.0) / correct_fish;
            tex_index = (int)(floor_ray * Sin + playerY) % 64 * 64 + (int)(floor_ray * Cos + playerX) % 64;
            if (tex_index > 63 * 64)
                tex_index = 63*64;
            r = texture_missing[tex_index][red];
            g = texture_missing[tex_index][green];
            b = texture_missing[tex_index][blue];
            r *= brightness[((int)(floor_ray * Sin + playerY)>>LIGHT_POW)*MAPY*(TILE/LIGHT_GRID) + ((int)(floor_ray * Cos + playerX)>>LIGHT_POW)][red];
            g *= brightness[((int)(floor_ray * Sin + playerY)>>LIGHT_POW)*MAPY*(TILE/LIGHT_GRID) + ((int)(floor_ray * Cos + playerX)>>LIGHT_POW)][green];
            b *= brightness[((int)(floor_ray * Sin + playerY)>>LIGHT_POW)*MAPY*(TILE/LIGHT_GRID) + ((int)(floor_ray * Cos + playerX)>>LIGHT_POW)][blue];
            glColor3f(r, g, b);
            glBegin(GL_POINTS);
            glVertex2i(ray * SCALE, hposition + end);
            glEnd();
            /*
            r = texture_missing[tex_index][red] / 2.0;
            g = texture_missing[tex_index][green] / 2.0;
            b = texture_missing[tex_index][blue];
            */
            r = g = b = 1;
            r *= brightness[((int)(floor_ray * Sin + playerY)>>LIGHT_POW)*MAPY*(TILE/LIGHT_GRID) + ((int)(floor_ray * Cos + playerX)>>LIGHT_POW)][red];
            g *= brightness[((int)(floor_ray * Sin + playerY)>>LIGHT_POW)*MAPY*(TILE/LIGHT_GRID) + ((int)(floor_ray * Cos + playerX)>>LIGHT_POW)][green];
            b *= brightness[((int)(floor_ray * Sin + playerY)>>LIGHT_POW)*MAPY*(TILE/LIGHT_GRID) + ((int)(floor_ray * Cos + playerX)>>LIGHT_POW)][blue];
            glColor3f(r, g, b);
            glBegin(GL_POINTS);
            glVertex2i(ray * SCALE, start -hposition);
            glEnd();
        }
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

void display() {
    current_frame = glutGet(GLUT_ELAPSED_TIME);
    delta_frames = current_frame - last_frame;
    last_frame = glutGet(GLUT_ELAPSED_TIME);

    check_inputs();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    doorf();
    //combine_light(player_light, false);
    //gen_light((int)(playerX), (int)(playerY), 1000.0f, player_light, 1, 1, 1, false);
    //combine_light(player_light, true);
    DDA();
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

void init() {
    glClearColor(0.3,0.3,0.3,0);
    gluOrtho2D(0,1024,HEIGHT,0);
    playerX = 300;
    playerY = 300;
    playerAngle = 0.0f;
    gen_light(3.5*TILE, 2.5*TILE, 10000.0f, lamp, 1, 0, 0, true);
    combine_light(lamp, true);
    gen_light(5.5 * TILE, 4.5 * TILE, 10000.0f, lamp, 0, 1, 0, true);
    combine_light(lamp, true);
    gen_light(3.5 * TILE, 4.5 * TILE, 10000.0f, lamp, 0, 0, 1, true);
    combine_light(lamp, true);
    FILE* fptr = NULL;
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
    int i;
    float base_angle = 0.5 * PI - SHIFT;
    radian_change(&base_angle);
    float baseCos = cos(base_angle);
    float side = RES * sin(base_angle) / sin(FOV);
    for (i = 0; i<RES; i++)
        angles[i] = acos((side - i * baseCos) / sqrt(side * side + i * i - 2 * side * i * baseCos));
    doors[0].x = 5;
    doors[0].y = 2;
    doors[0].exte = 64.0;
    doors[0].exte_rate = 0.0;
    doors[0].wait = 0;
    doors[1].x = 1;
    doors[1].y = 3;
    doors[1].exte = 64.0;
    doors[1].exte_rate = 0.0;
    doors[1].wait = 0;
}

int main(int argc, char* argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(RES * SCALE, HEIGHT);
    glutCreateWindow("Raycaster");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keydown);
    glutKeyboardUpFunc(keyup);
    glutMainLoop();
}
