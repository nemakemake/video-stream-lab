#include "Fft.h"

#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
}

void fft1d(std::vector<std::complex<double>> &a, bool invert)
{
    const int n = static_cast<int>(a.size());
    if (n <= 1)
        return;

    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }

    for (int len = 2; len <= n; len <<= 1) {
        const double ang = 2 * kPi / len * (invert ? -1 : 1);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (int j = 0; j < len / 2; ++j) {
                const std::complex<double> u = a[i + j];
                const std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (invert) {
        for (auto &x : a)
            x /= n;
    }
}

void fft2d(std::vector<std::vector<std::complex<double>>> &data)
{
    const int rows = static_cast<int>(data.size());
    const int cols = static_cast<int>(data[0].size());

    for (int r = 0; r < rows; ++r)
        fft1d(data[r], false);

    std::vector<std::vector<std::complex<double>>> transposed(
        cols, std::vector<std::complex<double>>(rows));
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            transposed[c][r] = data[r][c];

    for (int c = 0; c < cols; ++c)
        fft1d(transposed[c], false);

    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            data[r][c] = transposed[c][r];
}
