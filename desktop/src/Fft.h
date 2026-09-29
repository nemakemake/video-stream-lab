#pragma once

#include <complex>
#include <vector>

// Размер должен быть степенью двойки.
void fft1d(std::vector<std::complex<double>> &a, bool invert);
void fft2d(std::vector<std::vector<std::complex<double>>> &data);
