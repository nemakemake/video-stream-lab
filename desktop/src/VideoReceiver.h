#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QImage>
#include <QByteArray>
#include <QElapsedTimer>

// Принимает видеопоток по протоколу v1: [4 байта длины, big-endian][JPEG-байты],
// подряд, без разделителей. Работает как с реальным ESP32, так и с mock_server.py —
// протокол одинаковый, меняется только адрес/порт.
class VideoReceiver : public QObject
{
    Q_OBJECT

public:
    explicit VideoReceiver(QObject *parent = nullptr);

    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    double currentFps() const { return m_currentFps; }

signals:
    void frameReady(const QImage &frame);
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void fpsUpdated(double fps);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void tryParseBuffer();
    void updateFpsCounter();

    QTcpSocket *m_socket;
    QByteArray m_buffer;

    // Заголовок кадра: 4 байта длины. Пока не накопили — не знаем, сколько ждать.
    static constexpr int kHeaderSize = 4;
    quint32 m_expectedFrameSize = 0;
    bool m_haveHeader = false;

    QElapsedTimer m_fpsTimer;
    int m_framesSinceLastFpsUpdate = 0;
    double m_currentFps = 0.0;
};
