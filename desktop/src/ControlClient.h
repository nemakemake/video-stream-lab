#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTcpSocket>
#include <QByteArray>

// Control-канал протокола v1: JSON-команды по одной на строку, например
// {"cmd":"set","param":"exposure","value":300}. Прошивка (и mock_server.py)
// отвечают {"ack":true,...}. Видео сюда не попадает — оно идёт по отдельному порту.
class ControlClient : public QObject
{
    Q_OBJECT

public:
    explicit ControlClient(QObject *parent = nullptr);

    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    void setParameter(const QString &param, int value);
    // Просит у устройства текущие настройки; ответ придёт сигналом settingsReceived.
    void requestSettings();

signals:
    void connected();
    void disconnected();
    void settingsReceived(const QJsonObject &settings);
    void ackReceived(const QString &param, int value);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket;
    QByteArray m_buffer;
};
