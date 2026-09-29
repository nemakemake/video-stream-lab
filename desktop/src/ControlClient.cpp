#include "ControlClient.h"

#include <QJsonDocument>
#include <QJsonObject>

ControlClient::ControlClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &ControlClient::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ControlClient::disconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ControlClient::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this]() {
        emit errorOccurred(m_socket->errorString());
    });
}

void ControlClient::connectToHost(const QString &host, quint16 port)
{
    m_buffer.clear();
    m_socket->connectToHost(host, port);
}

void ControlClient::disconnectFromHost()
{
    m_socket->disconnectFromHost();
}

bool ControlClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ControlClient::setParameter(const QString &param, int value)
{
    if (!isConnected())
        return;

    const QJsonObject cmd{
        {QStringLiteral("cmd"), QStringLiteral("set")},
        {QStringLiteral("param"), param},
        {QStringLiteral("value"), value},
    };
    m_socket->write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n');
}

void ControlClient::requestSettings()
{
    if (!isConnected())
        return;

    const QJsonObject cmd{{QStringLiteral("cmd"), QStringLiteral("get")}};
    m_socket->write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n');
}

void ControlClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    int newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);
        if (line.isEmpty())
            continue;

        const QJsonObject reply = QJsonDocument::fromJson(line).object();
        if (reply.contains(QStringLiteral("settings"))) {
            emit settingsReceived(reply.value(QStringLiteral("settings")).toObject());
        } else if (reply.value(QStringLiteral("ack")).toBool()) {
            emit ackReceived(reply.value(QStringLiteral("param")).toString(),
                             reply.value(QStringLiteral("value")).toInt());
        } else if (reply.contains(QStringLiteral("error"))) {
            emit errorOccurred(reply.value(QStringLiteral("error")).toString());
        }
    }
}
