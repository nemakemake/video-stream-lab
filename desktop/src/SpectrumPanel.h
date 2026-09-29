#pragma once

#include <QFutureWatcher>
#include <QWidget>
#include <QImage>

class QLabel;

// 2D FFT спектр от кадра видео: grayscale, даунскейл до 256x256, FFT по строкам
// и столбцам, лог-амплитуда, fftshift в центр.
class SpectrumPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumPanel(QWidget *parent = nullptr);

public slots:
    void setFrame(const QImage &frame);

private:
    static QImage computeSpectrum(const QImage &frame);

    QLabel *m_spectrumLabel;
    QFutureWatcher<QImage> m_watcher;
    bool m_busy = false;

    static constexpr int kSize = 256;
};
