/**
 * @file simple_fft.h
 * @brief Simple FFT implementation for audio visualization
 */

#ifndef _SIMPLE_FFT_H_
#define _SIMPLE_FFT_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Complex number structure
typedef struct {
    float r;
    float i;
} complex_t;

/**
 * @brief Perform FFT on input data
 * @param input Array of complex numbers (input/output)
 * @param n Number of points (must be power of 2)
 */
void simple_fft(complex_t *input, int n);

/**
 * @brief Calculate magnitude of complex numbers
 * @param input Array of complex numbers
 * @param output Array of magnitudes (half size of input)
 * @param n Number of complex points
 */
void simple_fft_magnitude(complex_t *input, int16_t *output, int n);

#ifdef __cplusplus
}
#endif

#endif /* _SIMPLE_FFT_H_ */
