#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define CHANNELS 3
#define HEIGHT 64
#define WIDTH 64
#define SCALE 8
#define ZOOM 8
#define DZOOM 8.0
#define TURB 32

float noise[HEIGHT * WIDTH] = {0};

enum colors { red, green, blue };

void gen_noise() {
    int i, j;
    for (i=0;i<HEIGHT;i++) {
        for (j=0;j<WIDTH;j++)
            noise[i * WIDTH + j] = (rand() % 32768) / 32768.0;
    }
}

double smooth_noise(double x, double y, double zoom) {
    double fractX = x - (int)(x);
    double fractY = y - (int)(y);

    int x1 = ((int)(x) + WIDTH) % WIDTH;
    int y1 = ((int)(y) + HEIGHT) % HEIGHT;

    int x2 = (int)(x + WIDTH/zoom-1) % (int)(WIDTH/zoom);
    int y2 = (int)(y + HEIGHT/zoom-1) % (int)(HEIGHT/zoom);

    double val = 0.0;
    val += fractX * fractY * noise[x1 + y1 * WIDTH];
    val += (1 - fractX) * fractY * noise[x2 + y1 * WIDTH];
    val += fractX * (1 - fractY) * noise[x1 + y2 * WIDTH];
    val += (1 - fractX) * (1 - fractY) * noise[x2 + y2 * WIDTH];

    return val;
}

double turbulence(double x, double y, double size) {
    double val = 0.0;
    double initial_size = size;
    while (size>=1) {
        val += smooth_noise(x / size, y / size, size) * size;
        size /= 2.0;
    }

    return val / initial_size / 2.0;
}

void genmarble(float texture[][CHANNELS]) {
    gen_noise();
    double sineValue, xyValue;
    double xPeriod = 2.0;
    double yPeriod = 4.0;
    double turbPower = 8.0;

    int y, x;
    for (y=0;y<HEIGHT;y++) {
        for (x=0;x<WIDTH;x++) {
            xyValue = x * xPeriod / WIDTH + y * yPeriod / HEIGHT + turbPower * turbulence(x, y, TURB);
            sineValue = fabs(sin(xyValue * 3.14159));
            texture[y * WIDTH + x][green] = texture[y * WIDTH + x][blue] = texture[y * WIDTH + x][red] = sineValue;
        }
    }
}
