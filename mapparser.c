#include <stdio.h>
#include <stdbool.h>

#define MAXL 128
#define MAXNL 8

enum colors { red, green, blue };

int intpow(int num, int pow) {
    int num_in_pow = 1;
    for (;pow>0;pow--)
        num_in_pow *= num;
    return num_in_pow;
}

int stoint(char text[MAXNL], int base) {
    int num = 0, i, j;
    for (i=0;i<MAXL && text[i]!='\0';i++);
    for (j=--i;j>=0;j--) {
        if (text[j] >= '0' && text[j] <= '9')
            num += (text[j] - '0') * intpow(base, i-j);
        else if (text[j] >= 'A' && text[j] <= 'F' && base == 16)
            num += (text[j] - 'A' + 10) * intpow(base, i-j);
    }
    return num;
}

void parsel(FILE* fptr, int num, int out[], int color_pos) {
    int symbol = 0, wordnum = 0, offset = 0;
    char c = ' ', lc, word[MAXNL];
    bool comment = false, end = false;
    while (!end) {
        lc = c;
        if (lc == '\n' || lc == EOF) {
            break;
        }
        c = fgetc(fptr);
        if (comment || wordnum+offset >= num || (c == ' ' && lc == ' '))
            continue;
        if (lc == '/' && c == '/') {
            word[symbol-1] = '\0';
            if (color_pos == wordnum)
                out[wordnum+offset] = stoint(word, 16);
            else
                out[wordnum+offset] = stoint(word, 10);
            //printf("#%s#%d#%d\n", word, out[wordnum+offset], wordnum+offset);
            comment = true;
        }
        if ((c == ' ' || c == '\n' || c == 0x0D) && lc != ' ' || color_pos == wordnum && symbol>1) {
            word[symbol] = '\0';
            if (color_pos == wordnum)
                out[wordnum+offset] = stoint(word, 16);
            else
                out[wordnum+offset] = stoint(word, 10);
            //printf(" %s#%d#%d\n", word, out[wordnum+offset], wordnum+offset);
            if (color_pos == wordnum && symbol>1 && offset < 2) {
                offset++;
                word[0] = c;
                symbol = 0;
            }
            else {
                wordnum++;
                symbol = -1;
            }
        }
        else if (c != ' ' && c != '\n' && symbol < MAXNL)
            word[symbol] = c;
        symbol++;
    }
}
