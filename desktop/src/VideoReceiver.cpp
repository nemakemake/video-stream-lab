#include "VideoReceiver.h"

#include <QDataStream>

VideoReceiver::VideoReceiver(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, &VideoReceiver::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &VideoReceiver::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &VideoReceiver::disconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &VideoReceiver::onSocketError);

    m_fpsTimer.start();
}

void VideoReceiver::connectToHost(const QString &host, quint16 port)
{
    m_buffer.clear();
    m_haveHeader = false;
    m_expectedFrameSize = 0;
    m_socket->connectToHost(host, port);
}

void VideoReceiver::disconnectFromHost()
{
    m_socket->disconnectFromHost();
}

bool VideoReceiver::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void VideoReceiver::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    tryParseBuffer();
}

void VideoReceiver::tryParseBuffer()
{
    while (true) {
        if (!m_haveHeader) {
            if (m_buffer.size() < kHeaderSize)
                return;

            QDataStream stream(m_buffer.left(kHeaderSize));
            stream.setByteOrder(QDataStream::BigEndian);
            stream >> m_expectedFrameSize;
            m_buffer.remove(0, kHeaderSize);
            m_haveHeader = true;

            // защита от рассинхронизации протокола
            constexpr quint32 kMaxSaneFrameSize = 16 * 1024 * 1024;
            if (m_expectedFrameSize == 0 || m_expectedFrameSize > kMaxSaneFrameSize) {
                emit errorOccurred(QStringLiteral("Некорректный размер кадра в потоке (%1)")
                                        .arg(m_expectedFrameSize));
                disconnectFromHost();
                return;
            }
        }

        if (m_buffer.size() < static_cast<int>(m_expectedFrameSize))
            return;

        const QByteArray jpegData = m_buffer.left(m_expectedFrameSize);
        m_buffer.remove(0, m_expectedFrameSize);
        m_haveHeader = false;

        QImage frame;
        if (frame.loadFromData(jpegData, "JPEG")) {
            emit frameReady(frame);
            updateFpsCounter();
        } else {
            emit errorOccurred(QStringLiteral("Не удалось декодировать JPEG-кадр (%1 байт)")
                                    .arg(jpegData.size()));
        }
    }
}

void VideoReceiver::updateFpsCounter()
{
    ++m_framesSinceLastFpsUpdate;
    const qint64 elapsedMs = m_fpsTimer.elapsed();
    if (elapsedMs >= 1000) {
        m_currentFps = m_framesSinceLastFpsUpdate * 1000.0 / elapsedMs;
        m_framesSinceLastFpsUpdate = 0;
        m_fpsTimer.restart();
        emit fpsUpdated(m_currentFps);
    }
}

void VideoReceiver::onSocketError(QAbstractSocket::SocketError /*error*/)
{
    emit errorOccurred(m_socket->errorString());
}
