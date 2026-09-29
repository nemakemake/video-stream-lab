#include "SpectrumPanel.h"
#include "Fft.h"

#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>

SpectrumPanel::SpectrumPanel(QWidget *parent)
    : QWidget(parent)
    , m_spectrumLabel(new QLabel(this))
{
    m_spectrumLabel->setAlignment(Qt::AlignCenter);
    m_spectrumLabel->setMinimumSize(kSize, kSize);
    m_spectrumLabel->setStyleSheet(QStringLiteral("background-color: #101010;"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_spectrumLabel);

    connect(&m_watcher, &QFutureWatcher<QImage>::finished, this, [this] {
        m_spectrumLabel->setPixmap(QPixmap::fromImage(m_watcher.result()));
        m_busy = false;
    });
}

void SpectrumPanel::setFrame(const QImage &frame)
{
    // FFT считается в отдельном потоке; если предыдущий расчёт ещё не готов —
    // просто пропускаем кадр, а не ставим в очередь (спектру не нужен каждый кадр,
    // а очередь на UI-потоке приводила к зависанию при пачках кадров).
    if (m_busy)
        return;

    m_busy = true;
    m_watcher.setFuture(QtConcurrent::run(&SpectrumPanel::computeSpectrum, frame));
}

QImage SpectrumPanel::computeSpectrum(const QImage &frame)
{
    const QImage gray = frame
                             .convertToFormat(QImage::Format_Grayscale8)
                             .scaled(kSize, kSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    std::vector<std::vector<std::complex<double>>> data(
        kSize, std::vector<std::complex<double>>(kSize));
    for (int y = 0; y < kSize; ++y) {
        const uchar *line = gray.constScanLine(y);
        for (int x = 0; x < kSize; ++x)
            data[y][x] = std::complex<double>(line[x], 0.0);
    }

    fft2d(data);

    std::vector<std::vector<double>> magnitude(kSize, std::vector<double>(kSize));
    double maxVal = 0.0;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const double m = std::log1p(std::abs(data[y][x]));
            magnitude[y][x] = m;
            maxVal = std::max(maxVal, m);
        }
    }

    QImage result(kSize, kSize, QImage::Format_Grayscale8);
    const int half = kSize / 2;
    for (int y = 0; y < kSize; ++y) {
        uchar *line = result.scanLine(y);
        const int sy = (y + half) % kSize;
        for (int x = 0; x < kSize; ++x) {
            const int sx = (x + half) % kSize;
            const double normalized = maxVal > 0.0 ? magnitude[sy][sx] / maxVal : 0.0;
            line[x] = static_cast<uchar>(std::clamp(normalized * 255.0, 0.0, 255.0));
        }
    }
    return result;
}
