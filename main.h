#ifndef MAIN_H
# define MAIN_H
#include "triangle.h"
#define CHANNELS 3
void parsel(FILE*,int,int[],int);
void genmarble(float[][CHANNELS]);
int parse(FILE*, float[][CHANNELS]);
#endif
