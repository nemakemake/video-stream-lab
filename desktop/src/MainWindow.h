#pragma once

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class VideoReceiver;
class ControlClient;
class SettingsPanel;

// Milestone 1: подключение к видеопотоку (реальному ESP32 или mock_server.py),
// отображение кадров + live FPS. Дальше сюда будут добавляться вкладки анализаторов
// (гистограмма, waveform/vectorscope, спектр, метрики канала).
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onConnectClicked();
    void onFrameReady(const QImage &frame);
    void onFpsUpdated(double fps);
    void onError(const QString &message);
    void onConnected();
    void onDisconnected();

private:
    void setupUi();

    VideoReceiver *m_receiver;
    ControlClient *m_control;
    SettingsPanel *m_settingsPanel;

    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QPushButton *m_connectButton;
    QLabel *m_videoLabel;
    QLabel *m_statusLabel;

    // Запоминаем последнюю ошибку, чтобы onDisconnected() (который срабатывает
    // сразу вслед за ошибкой, если она привела к разрыву) не затирал её текст.
    QString m_lastError;
};
