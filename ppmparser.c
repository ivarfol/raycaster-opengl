#include <stdio.h>

#define HEIGHT 64
#define WIDTH 64
#define SEP 0x0A

float texture[HEIGHT * WIDTH][3] = {0};

enum colors { red, green, blue };

void parse(FILE* fptr) {
    int c = 0;
    int symbol;
    for (symbol=0;symbol<3;) {
        if ((c = fgetc(fptr)) == SEP)
            symbol++;
    }
    int colorn, color_part;
    for (colorn=0;colorn<HEIGHT * WIDTH;colorn++) {
        for (color_part=red;color_part<=blue;color_part++) {
            c = fgetc(fptr);
            texture[colorn][color_part] = c / 255.0;
        }
    }
}
