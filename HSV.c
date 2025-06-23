#include <stdio.h>
#include <stdbool.h>
#include <math.h>

void mod_brightness(float* r, float* g, float* b, float brightness) {
    float H, V, S;
    float R, G, B;
    float alpha, beta, gamma;
    float* max;
    float* min;
    float delta;
    bool H_defined = true;
    if (*r > *g) {
        if (*r > *b) {
            max = r;
            if (*b > *g)
                min = g;
            else
                min = b;
        }
        else {
            max = b;
            min = g;
        }
    }
    else {
        if (*g > *b) {
            max = g;
            if (*r > *b)
                min = b;
            else
                min = r;
        }
        else {
            max = b;
            min = r;
        }
    }
    delta = *max - *min;

    //to HSV
    V = *max;
    if (V == 0.0f)
        S = 0.0f;
    else
        S = delta / V;
    if (delta < 0.001)
        H_defined = false;
    else if (max == r)
        H = (*g - *b) / delta;
    else if (max == g)
        H = (*b - *r) / delta + 2;
    else if (max == b)
        H = (*r - *g) / delta + 4;

    //change brightness
    V *= brightness;

    //to RGB
    if (H_defined) {
        if (H < 0)
            H = fmod(H, 6.0f) + 6;
        else
            H = fmod(H, 6.0f);
    }
    alpha = V * (1 - S);
    if (H_defined)
        beta = V * (1 - (H - floor(H)) * S);
    if (H_defined)
        gamma = V * (1 - (1 - (H - floor(H))) * S);

    if (!H_defined) {
        *r = V;
        *g = V;
        *b = V;
    }
    else if (H < 1) {
        *r = V;
        *g = gamma;
        *b = alpha;
    }
    else if (H < 2) {
        *r = beta;
        *g = V;
        *b = alpha;
    }
    else if (H < 3) {
        *r = alpha;
        *g = V;
        *b = gamma;
    }
    else if (H < 4) {
        *r = alpha;
        *g = beta;
        *b = V;
    }
    else if (H < 5) {
        *r = gamma;
        *g = alpha;
        *b = V;
    }
    else if (H < 6) {
        *r = V;
        *g = alpha;
        *b = beta;
    }
}
