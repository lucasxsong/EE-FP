#ifndef FFT_H
#define FFT_H

#include <complex.h>
#include <stddef.h>

#define cplx float _Complex
/**
 * @brief FFT algorithm (forward transform)
 *
 * This function computes forward radix-2 fast Fourier transform (FFT).
 * The output is written in-place over the input.
 *
 * @param vector An array of @p N complex values in single-precision floating-point format.
 * @param N The size of the transform must be a power of two.
 *
 * @return Zero for success.
 */
int fft(cplx *vector, size_t N);

#endif
