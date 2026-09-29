#pragma once

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class VideoReceiver;
class ControlClient;
class SettingsPanel;
class SpectrumPanel;

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
    SpectrumPanel *m_spectrumPanel;

    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QPushButton *m_connectButton;
    QLabel *m_videoLabel;
    QLabel *m_statusLabel;

    QString m_lastError;
};
