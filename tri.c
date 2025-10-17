#include <stdio.h>
#include <GL/glut.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

typedef struct {
    int x;
    int y;
} point;

typedef struct {
    int* ptrx;
    int length;
} interp;

enum colors { red, green, blue };

void swap_p(point *p1, point *p2) {
    point tmp = *p1;
    *p1 = *p2;
    *p2 = tmp;
}

interp interpolate(point p0, point p1) {
    interp out;
    out.length = p1.y - p0.y;
    out.ptrx = malloc(out.length * sizeof(int));
    int i;
    double dx = (p0.x - p1.x) / (double)(p0.y - p1.y);
    for (i=0;i<out.length;i++) {
        out.ptrx[i] = (int)round(p0.x + dx * i);
        //printf("%d %d\n", out.ptrx[i], i);
    }
    //printf("%d %f %d\n", out.length, dx, i);
    return out;
}

void filled_tr(point points_[3], bool texture[], int width) {
    point points[3];
    memcpy(points, points_, 3 * sizeof(point));
    if (points[1].y < points[0].y) {
        swap_p(&points[1], &points[0]);
    }
    if (points[2].y < points[0].y) {
        swap_p(&points[2], &points[0]);
    }
    if (points[2].y < points[1].y) {
        swap_p(&points[2], &points[1]);
    }
    interp x01 = interpolate(points[0], points[1]);
    interp x12 = interpolate(points[1], points[2]);
    interp x02 = interpolate(points[0], points[2]);
    int tlength = x01.length + x12.length + x02.length - 1;
    int x012[tlength];
    memcpy(x012, x01.ptrx, x01.length * sizeof(int));
    free(x01.ptrx);
    x01.ptrx = NULL;
    memcpy(x012 + x01.length, x12.ptrx, x12.length * sizeof(int));
    free(x12.ptrx);
    x12.ptrx = NULL;
    memcpy(x012 + x01.length + x12.length, x02.ptrx, (x02.length - 1) * sizeof(int));
    int m = floor(tlength / 2.0);
    int q = floor(m / 2.0);
    int* xleft;
    int* xright;
    if (x02.ptrx[m] < x012[m] || x02.ptrx[q] < x012[q]) {
        xleft = x02.ptrx;
        xright = x012;
    }
    else {
        xleft = x012;
        xright = x02.ptrx;
    }

    int i, j;
    for (i=points[0].y;i<points[2].y;i++) {
        for (j=xleft[i - points[0].y];j<xright[i-points[0].y]+1;j++) {
            texture[j + i * width] = 1;
        }
    }
    free(x02.ptrx);
    x02.ptrx = NULL;
}
