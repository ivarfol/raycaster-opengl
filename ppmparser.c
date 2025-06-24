#include <stdio.h>

#define HEIGHT 64
#define WIDTH 64
#define SEP 0x0A
#define MAXL 128

enum colors { red, green, blue };

int ipow(int num, int pow) {
    int num_in_pow = 1;
    for (;pow>0;pow--)
        num_in_pow *= num;
    return num_in_pow;
}

int stoi(char text[MAXL]) {
    int num=0, i, j;
    for (i=0;i<MAXL && text[i]!='\0';i++);
    for (j=--i;j>=0;j--)
        num += (text[j] - '0') * ipow(10, i-j);
    return num;
}

void parse(FILE* fptr, float texture[][3]) {
    int c;
    int line, symbol, depth;
    int Ylength = 0;
    int Xlength = 0;
    char text[MAXL];
    for (line=0;line<3;) {
        for (symbol=0;symbol<MAXL;symbol++) {
            c = fgetc(fptr);
            if (c == '#')
                break;
            if (c == '\n' || c == ' ') {
                if (line == 1) {
                    if (Ylength == 0)
                        Ylength = stoi(text);
                    else
                        Xlength = stoi(text);
                }
                else if (line == 2)
                    depth = stoi(text);
                if (c == SEP) {
                    line++;
                    break;
                }
                c = fgetc(fptr);
                symbol = 0;
            }
            text[symbol] = c;
        }
    }
    int colorn, color_part;
    for (colorn=0;colorn<HEIGHT * WIDTH;colorn++) {
        for (color_part=red;color_part<=blue;color_part++) {
            c = fgetc(fptr);
            if (colorn==0 && c == '#') {
                while ((c=fgetc(fptr)) != SEP);
                c = fgetc(fptr);
            }
            texture[colorn][color_part] = c / (float)depth;
        }
    }
}
