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
#define DOORN 1
#define MAP_SCALE 16

#define HEIGHT 512

typedef struct {
    int w, a, s, d, q, e, m, f, space, shift, p;
} keys;
keys oldkeys;
keys newkeys;

typedef struct {
    int x, y, wait;
    float exte, exte_rate;
} door_struct;
door_struct doors[1];

float playerX, playerY, playerAngle;
unsigned int mapX = 8, mapY = 8;
bool show_map = true;
float move_direction_v, move_direction_h;

float angles[RES];

short unsigned int map[] =
{
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 0, 1,
    1, 3, 1, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
};

void radian_change(float *a) {
    if (*a > 2 * PI)
        *a -= 2 * PI;
    else if (*a < 0)
        *a += 2 * PI;
    if (*a > 2 * PI || *a < 0)
        radian_change(a);
}

door_struct* getDoor(int x, int y) {
    int door_index;
    for (door_index=0;door_index<DOORN || doors[door_index].x == x && doors[door_index].y == y;door_index++);
    return(&doors[--door_index]);
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
    float tmpX = *positionX + cos(move_direction) * modifier;
    float tmpY = *positionY + sin(move_direction) * modifier;
    int maptmpX = (int)tmpX>>6;
    int maptmpY = (int)tmpY>>6;
    if (map[maptmpX + maptmpY * mapX] == 2 || map[maptmpX + maptmpY * mapX] == 3)
        door = getDoor(maptmpX, maptmpY);
    if (map[((int)tmpY>>6) * mapX + ((int)(*positionX)>>6)] == 0 || door != NULL && door->exte == 0.0)
        *positionY = tmpY;
    if (map[((int)(*positionY)>>6) * mapX + ((int)tmpX>>6)] == 0 || door != NULL && door->exte == 0.0)
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
        playerAngle -= 0.1;
        radian_change(&playerAngle);
    }
    if (newkeys.e) {
        playerAngle += 0.1;
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
        movef(PLAYER_SPEED, move_direction_h, &playerX, &playerY, 1.0f);
    }
    else if (move_direction_v >= 0.0) {
        move_direction_v += playerAngle;
        radian_change(&move_direction_v);
        movef(PLAYER_SPEED, move_direction_v, &playerX, &playerY, 1.0f);
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

        double Cos = cos(angle); //what?
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
        //printf("H%f V%f\n", deltadistH, deltadistV);
        int raymapXH, raymapYH, raymapH;
        int raymapXV, raymapYV, raymapV;
        door_struct* doorH;
        door_struct* doorV;
        while (dofV < DOF || dofH < DOF) {
            if (dofH < DOF && distH <= distV) {
                raymapXH = (int)(rayXH)>>6;
                raymapYH = (int)(rayYH)>>6;
                raymapH = raymapXH + raymapYH * mapX;
                if (raymapH > 0 && raymapH < mapX * mapY && map[raymapH] == 1) {
                    dofH = DOF;
                    break;
                }
                else if (raymapH > 0 && raymapH < mapX * mapY && map[raymapH] == 3) {
                    //printf("%f < %f\n", doors[door_indexH].x * 64 + doors[door_indexH].exte - 34, rayXH + 0.5 * deltaXH);
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
                    break;
                }
                else if (raymapV > 0 && raymapV < mapX * mapY && map[raymapV] == 2) {
                    doorH = getDoor(raymapXH, raymapYH);
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
            else //printf("dofV %d\ndofH %d\ndistV %f\ndistH %f\n", dofV, dofH, distV, distH);
                break;
        }
        //printf("%d %d\n", dofV, dofH);
        //printf("%d raymapXV %d raymapYV\n%d raymapXH %d raymapYH\n", raymapXV, raymapYV, raymapXH, raymapYH);

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


        if (distV <= distH) {
            distH = distV;
            glColor3f(0.9, 0, 0);
        }
        else
            glColor3f(0.7, 0, 0);
        distH *= correct_fish;
        float line_height = 64 * HEIGHT / distH;
        int start, end;
        if (line_height > HEIGHT) {
            start = 0;
            end = HEIGHT;
        }
        else {
            start = HEIGHT / 2 - 0.5 * line_height;
            end = HEIGHT / 2 + 0.5 * line_height;
        }
        int hposition;
        for (hposition=start; hposition<end;hposition++) {
            glPointSize(SCALE);
            glBegin(GL_POINTS);
            glVertex2i(ray * SCALE, hposition);
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
            door->exte += door->exte_rate;
            if (door->exte < 0) {
                door->exte_rate = 0.5;
                door->exte = 0.0;
                door->wait = 300;
            }
        }
        else if (door->wait > 0)
            door->wait -= 1;
        else if ((((int)playerX)>>6) != door->x || (((int)playerY)>>6) != door->y) {
            //printf("%d %d\n", (((int)playerX)>>6), (((int)playerY)>>6));
            door->exte += door->exte_rate;
            if (door->exte > 64) {
                door->exte_rate = 0.0;
                door->exte = 64.0;
            }
        }
        door++;
    }
}

void display() {
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
    int i;
    float base_angle = 0.5 * PI - SHIFT;
    radian_change(&base_angle);
    float baseCos = cos(base_angle);
    float side = RES * sin(base_angle) / sin(FOV);
    for (i = 0; i<RES; i++)
        angles[i] = acos((side - i * baseCos) / sqrt(side * side + i * i - 2 * side * i * baseCos));
    doors[0].x = 1;
    doors[0].y = 3;
    doors[0].exte = 64.0;
    doors[0].exte_rate = 0.0;
    doors[0].wait = 0;
}

int main(int argc, char* argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1024, 512);
    glutCreateWindow("Raycaster");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keydown);
    glutKeyboardUpFunc(keyup);
    glutMainLoop();
}
