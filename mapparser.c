#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAXL 128
#define MAXNL 8

enum colors { red, green, blue };

int intpow(int num, int pow) {
    int num_in_pow = 1;
    for (;pow>0;pow--)
        num_in_pow *= num;
    return num_in_pow;
}

int stoint(char text[MAXNL], int base, bool *fail) {
    int num = 0, i, j;
    for (i=0;i<MAXL && text[i]!='\0';i++);
    if (i == 0 && text[i] == '-')
        i--;
    for (j=--i;j>=0;j--) {
        if (j == 0 && text[j] == '-')
            num *= -1;
        else if (text[j] >= '0' && text[j] <= '9')
            num += (text[j] - '0') * intpow(base, i-j);
        else if (text[j] >= 'A' && text[j] <= 'F' && base == 16)
            num += (text[j] - 'A' + 10) * intpow(base, i-j);
        else {
            *fail = true;
            return -1;
        }
    }
    return num;
}

void parsel(FILE* fptr, int num, int out[], int color_pos) {
    int symbol = 0, wordnum = 0, offset = 0;
    char c = ' ', lc, word[MAXNL];
    bool comment = false, end = false, color = false;
    bool fail = false;
    while (!end) {
        lc = c;
        if (lc == '\n' || lc == EOF) {
            break;
        }
        c = fgetc(fptr);
        if (comment || (c == ' ' && lc == ' '))
            continue;
        if (wordnum + offset > num) {
            printf("Invalid map file, too many numbers in a line\n");
            exit(1);
        }
        if (lc == '/' && c == '/') {
            word[symbol-1] = '\0';
            //printf("word %s wordnum %d offset %d num %d color_pos %d\n", word, wordnum, offset, num, color_pos);
            if (!color && symbol > 1) {
                out[wordnum + offset] = stoint(word, 10, &fail);
                if (fail) {
                    printf("Invalid map file, not a number err1\n");
                    exit(1);
                }
                else if (wordnum + offset + 1 > num) {
                    printf("Invalid map file, too many numbers in a line\n");
                    exit(1);
                }
            }
            //printf("#%s#%d#%d\n", word, out[wordnum+offset], wordnum+offset);
            comment = true;
        }
        if ((c == ' ' || c == '\n' || c == 0x0D) && lc != ' ' && lc != 0x0D || color_pos == wordnum && symbol>1) {
            word[symbol] = '\0';
            if (color_pos == wordnum)
                out[wordnum+offset] = stoint(word, 16, &fail);
            else
                out[wordnum+offset] = stoint(word, 10, &fail);
            if (fail) {
                printf("Invalid map file, not a number err2\n");
                exit(1);
            }
            //printf(" %s#%d#%d\n", word, out[wordnum+offset], wordnum+offset);
            if (color_pos == wordnum && symbol>1 && offset < 2) {
                offset++;
                word[0] = c;
                symbol = 0;
                if (offset == 2)
                    color = true;
            }
            else {
                wordnum++;
                symbol = -1;
            }
        }
        else if (c != ' ' && c != '\n' && symbol < MAXNL) {
            word[symbol] = c;
            if (offset == 2)
                color = false;
        }
        symbol++;
    }
}
