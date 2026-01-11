/**
 * @file simple_fft.c
 * @brief Simple recursive radix-2 FFT implementation
 */

#include "simple_fft.h"
#include <math.h>

#define PI 3.14159265358979323846

static void _fft_recursive(complex_t *buf, int n)
{
    if (n <= 1)
        return;

    // Split into even and odd
    int       half = n / 2;
    complex_t even[half];
    complex_t odd[half];

    for (int i = 0; i < half; i++) {
        even[i] = buf[2 * i];
        odd[i]  = buf[2 * i + 1];
    }

    _fft_recursive(even, half);
    _fft_recursive(odd, half);

    for (int k = 0; k < half; k++) {
        float     angle = -2 * PI * k / n;
        complex_t t;
        t.r = cos(angle) * odd[k].r - sin(angle) * odd[k].i;
        t.i = cos(angle) * odd[k].i + sin(angle) * odd[k].r;

        buf[k].r = even[k].r + t.r;
        buf[k].i = even[k].i + t.i;

        buf[k + half].r = even[k].r - t.r;
        buf[k + half].i = even[k].i - t.i;
    }
}

void simple_fft(complex_t *input, int n)
{
    _fft_recursive(input, n);
}

void simple_fft_magnitude(complex_t *input, int16_t *output, int n)
{
    // We only need the first n/2 magnitudes (Nyquist)
    for (int i = 0; i < n / 2; i++) {
        float mag = sqrt(input[i].r * input[i].r + input[i].i * input[i].i);
        // Scale down fitting into int16 for display
        output[i] = (int16_t)(mag / 10.0f);
    }
}
