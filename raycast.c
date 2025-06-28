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
#define LIGHT_GRID 16

#define MIN_LIGHT_DIST 128

#define TEXWIDTH 64
#define TILE 64

#define PLHEIGHT 32

#define HEIGHT 512
#define MAPX 8
#define MAPY 8

#define LIGHT_POS 4 //light source

float texture_one[64 * 64][3];
float texture_missing[64 * 64][3];

extern int parse(FILE* fptr, float texture[][3]);
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
} keys;
keys oldkeys;
keys newkeys;

typedef struct {
    int x, y, wait;
    float exte, exte_rate;
} door_struct;
door_struct doors[DOORN];

float playerX, playerY, playerAngle;
unsigned int mapX = 8, mapY = 8;
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

float brightness[MAPX * MAPY * (TILE / LIGHT_GRID)*2] = {1};

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

void gen_light() {
}

void drawMap() {
    int x, y, tileX, tileY;
    for (y=0;y<mapY;y++) {
        for (x=0;x<mapX;x++) {
            if (map[y*mapX + x] == 1) {
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
            else if (map[y*mapX + x] == 3) {
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
    int tmpYmap = map[((int)tmpY>>6) * mapX + ((int)(*positionX)>>6)];
    int tmpXmap = map[((int)(*positionY)>>6) * mapX + ((int)tmpX>>6)];
    int oldposY = *positionY;
    if (tmpYmap == 3 || tmpYmap == 2)
        door = getDoor(((int)(*positionX)>>6), ((int)tmpY>>6));
    if (tmpYmap == 0 || door != NULL && door->exte == 0.0)
        *positionY = tmpY;
    door = NULL;
    if (tmpXmap == 3 || tmpXmap == 2)
        door = getDoor(((int)tmpX>>6), ((int)(oldposY)>>6));
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
            rayYH = ((((int)playerY)>>6)<<6) - 0.0001;
            rayXH = playerX - (playerY - rayYH) * invTan;
            distH = - (playerY - rayYH) / Sin;
        }
        else if (Sin > 0.0001) {
            rayYH = ((((int)playerY)>>6)<<6) + 64;
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
            rayXV = ((((int)playerX)>>6)<<6) + 64;
            rayYV = playerY - (playerX - rayXV) * Tan;
            distV = - (playerX - rayXV) / Cos;
        }
        else if (Cos < -0.0001) {
            rayXV = ((((int)playerX)>>6)<<6) - 0.001;
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
        raymapX = (int)(rayXH)>>6;
        raymapY = (int)(rayYH)>>6;
        if (map[raymapX + raymapY * mapX] == 2 || map[raymapX + raymapY * mapX] == 3) {
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
        double Tan = Sin / Cos;
        double invTan = 1 / Tan;

        //up/down
        if (Sin < -0.0001) {
            deltaYH = - 64;
            deltaXH = - 64 * invTan;
            deltadistH = - 64 / Sin;
            rayYH = ((((int)playerY)>>6)<<6) - 0.0001;
            rayXH = playerX - (playerY - rayYH) * invTan;
            distH = - (playerY - rayYH) / Sin;
            dofH = 0;
        }
        else if (Sin > 0.0001) {
            deltaYH = 64;
            deltaXH = 64 * invTan;
            deltadistH = 64 / Sin;
            rayYH = ((((int)playerY)>>6)<<6) + 64;
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
            deltaXV = 64;
            deltaYV = 64 * Tan;
            deltadistV = 64 / Cos;
            rayXV = ((((int)playerX)>>6)<<6) + 64;
            rayYV = playerY - (playerX - rayXV) * Tan;
            distV = - (playerX - rayXV) / Cos;
            dofV = 0;
        }
        else if (Cos < -0.0001) {
            deltaXV = - 64;
            deltaYV = - 64 * Tan;
            deltadistV = - 64 / Cos;
            rayXV = ((((int)playerX)>>6)<<6) - 0.001;
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
                raymapXH = (int)(rayXH)>>6;
                raymapYH = (int)(rayYH)>>6;
                raymapH = raymapXH + raymapYH * mapX;
                if (raymapH > 0 && raymapH < mapX * mapY && map[raymapH] == 1) {
                    dofH = DOF;
                    doorH = NULL;
                    break;
                }
                else if (raymapH > 0 && raymapH < mapX * mapY && map[raymapH] == 3) {
                    doorH = getDoor(raymapXH, raymapYH);
                    if (doorH->x * 64 - doorH->exte + 64 < rayXH + 0.5 * deltaXH) {
                        dofH = DOF;
                        distH += 0.5 * deltadistH;
                        rayXH += 0.5 * deltaXH;
                        rayYH += 0.5 * deltaYH;
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
                    rayXH += deltaXH;
                    rayYH += deltaYH;
                    distH += deltadistH;
                    dofH++;
                }
            }
            else if (dofV < DOF && distV < distH) {
                raymapXV = (int)(rayXV)>>6;
                raymapYV = (int)(rayYV)>>6;
                raymapV = raymapXV + raymapYV * mapX;
                if (raymapV > 0 && raymapV <mapX * mapY && map[raymapV] == 1) {
                    dofV = DOF;
                    doorV = NULL;
                    break;
                }
                else if (raymapV > 0 && raymapV < mapX * mapY && map[raymapV] == 2) {
                    doorV = getDoor(raymapXV, raymapYV);
                    if (doorV->y * 64 - doorV->exte + 64 < rayYV + 0.5 * deltaYV) {
                        dofV = DOF;
                        distV += 0.5 * deltadistV;
                        rayXV += 0.5 * deltaXV;
                        rayYV += 0.5 * deltaYV;
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
                    rayXV += deltaXV;
                    rayYV += deltaYV;
                    distV += deltadistV;
                    dofV++;
                }
            }
            else
                break;
        }

        if (show_map) {
            drawMap();
            if (distV <= distH) {
                glColor3f(0, 1, 0);
                glLineWidth(1);
                glBegin(GL_LINES);
                glVertex2i(playerX / 64 * MAP_SCALE, playerY / 64 * MAP_SCALE);
                glVertex2i(rayXV / 64 * MAP_SCALE, rayYV / 64 * MAP_SCALE);
                glEnd();
            }
            else {
                glColor3f(0, 0, 1);
                glLineWidth(1);
                glBegin(GL_LINES);
                glVertex2i(playerX / 64 * MAP_SCALE, playerY / 64 * MAP_SCALE);
                glVertex2i(rayXH / 64 * MAP_SCALE, rayYH / 64 * MAP_SCALE);
                glEnd();
            }
        }


        float bright = 1.0;
        float textureX;
        if (distV <= distH) {
            distH = distV;
            bright = 0.7;
            if (doorV == NULL) {
                if (Cos > 0)
                    textureX = (int)rayYV % 64;
                else
                    textureX = 63 - (int)rayYV % 64;
            }
            else
                textureX = -64 + (int)rayYV % 64 + ceil(doorV->exte);
            doorH = doorV;
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
        float brightness = 10000.0 / light_dist / light_dist;
        if (brightness > 1)
            brightness = 1.0f;
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
            glColor3f(r * brightness, g * brightness, b * brightness);
            glPointSize(SCALE);
            glBegin(GL_POINTS);
            glVertex2i(ray * SCALE, hposition);
            glEnd();
            textureY += deltatextureY;
        }
        float floor_ray;
        for (hposition = 0;hposition<HEIGHT - end;hposition++) {
            floor_ray = floor_const / (hposition + end - HEIGHT / 2.0) / correct_fish;
            float brightness = 10000.0 / floor_ray / floor_ray;
            if (brightness > 1)
                brightness = 1.0f;
            if (light_dist < MIN_LIGHT_DIST)
                light_dist = MIN_LIGHT_DIST;
            tex_index = (int)(floor_ray * Sin + playerY) % 64 * 64 + (int)(floor_ray * Cos + playerX) % 64;
            if (tex_index > 63 * 64)
                tex_index = 63*64;
            r = texture_missing[tex_index][red];
            g = texture_missing[tex_index][green];
            b = texture_missing[tex_index][blue];
            glColor3f(r * brightness, g * brightness, b * brightness);
            glBegin(GL_POINTS);
            glVertex2i(ray * SCALE, hposition + end);
            glEnd();
            r = texture_missing[tex_index][red] / 2.0;
            g = texture_missing[tex_index][green] / 2.0;
            b = texture_missing[tex_index][blue];
            glColor3f(r * brightness, g * brightness, b * brightness);
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
}

void init() {
    glClearColor(0.3,0.3,0.3,0);
    gluOrtho2D(0,1024,HEIGHT,0);
    playerX = 300;
    playerY = 300;
    playerAngle = 0.0f;
    gen_light();
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
