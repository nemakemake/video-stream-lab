#include "MainWindow.h"
#include "ControlClient.h"
#include "SettingsPanel.h"
#include "VideoReceiver.h"

#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>

// Control-порт по протоколу v1 (видео — 3333, выбирается в UI).
static constexpr quint16 kControlPort = 3334;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_receiver(new VideoReceiver(this))
    , m_control(new ControlClient(this))
    , m_settingsPanel(new SettingsPanel(this))
{
    setupUi();

    connect(m_settingsPanel, &SettingsPanel::parameterChanged, m_control, &ControlClient::setParameter);
    connect(m_control, &ControlClient::connected, this, [this] {
        m_settingsPanel->setControlAvailable(true);
        m_control->requestSettings();
    });
    connect(m_control, &ControlClient::settingsReceived, m_settingsPanel, &SettingsPanel::applySettings);
    connect(m_control, &ControlClient::disconnected, this, [this] { m_settingsPanel->setControlAvailable(false); });
    connect(m_control, &ControlClient::errorOccurred, this, [this](const QString &message) {
        m_statusLabel->setText(QStringLiteral("Control: %1").arg(message));
    });

    connect(m_receiver, &VideoReceiver::frameReady, this, &MainWindow::onFrameReady);
    connect(m_receiver, &VideoReceiver::fpsUpdated, this, &MainWindow::onFpsUpdated);
    connect(m_receiver, &VideoReceiver::errorOccurred, this, &MainWindow::onError);
    connect(m_receiver, &VideoReceiver::connected, this, &MainWindow::onConnected);
    connect(m_receiver, &VideoReceiver::disconnected, this, &MainWindow::onDisconnected);
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Video Stream Lab — Milestone 1"));
    resize(900, 700);

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);

    auto *connectionRow = new QHBoxLayout();
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), central);
    m_portSpin = new QSpinBox(central);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(3333); // видео-порт по протоколу v1, см. README
    m_connectButton = new QPushButton(QStringLiteral("Подключиться"), central);

    connectionRow->addWidget(new QLabel(QStringLiteral("Хост:"), central));
    connectionRow->addWidget(m_hostEdit);
    connectionRow->addWidget(new QLabel(QStringLiteral("Порт:"), central));
    connectionRow->addWidget(m_portSpin);
    connectionRow->addWidget(m_connectButton);
    connectionRow->addStretch();

    m_videoLabel = new QLabel(QStringLiteral("Нет подключения"), central);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(640, 480);
    m_videoLabel->setStyleSheet(QStringLiteral("background-color: #202020; color: #a0a0a0;"));

    m_statusLabel = new QLabel(QStringLiteral("Отключено"), central);

    rootLayout->addLayout(connectionRow);
    rootLayout->addWidget(m_videoLabel, 1);
    rootLayout->addWidget(m_statusLabel);

    setCentralWidget(central);

    auto *settingsDock = new QDockWidget(QStringLiteral("Настройки потока"), this);
    settingsDock->setWidget(m_settingsPanel);
    settingsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, settingsDock);

    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
}

void MainWindow::onConnectClicked()
{
    if (m_receiver->isConnected()) {
        m_receiver->disconnectFromHost();
        m_control->disconnectFromHost();
        return;
    }

    m_lastError.clear();
    m_statusLabel->setText(QStringLiteral("Подключение..."));
    m_receiver->connectToHost(m_hostEdit->text(), static_cast<quint16>(m_portSpin->value()));
    m_control->connectToHost(m_hostEdit->text(), kControlPort);
}

void MainWindow::onFrameReady(const QImage &frame)
{
    m_videoLabel->setPixmap(QPixmap::fromImage(frame).scaled(
        m_videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::onFpsUpdated(double fps)
{
    m_statusLabel->setText(QStringLiteral("Подключено — %1 FPS").arg(fps, 0, 'f', 1));
}

void MainWindow::onError(const QString &message)
{
    m_lastError = message;
    m_statusLabel->setText(QStringLiteral("Ошибка: %1").arg(message));
}

void MainWindow::onConnected()
{
    m_connectButton->setText(QStringLiteral("Отключиться"));
    m_statusLabel->setText(QStringLiteral("Подключено"));
}

void MainWindow::onDisconnected()
{
    // Видео пропало — control без видео бесполезен и держит единственный слот на ESP32.
    m_control->disconnectFromHost();
    m_connectButton->setText(QStringLiteral("Подключиться"));
    m_statusLabel->setText(m_lastError.isEmpty()
                                ? QStringLiteral("Отключено")
                                : QStringLiteral("Отключено (%1)").arg(m_lastError));
    m_videoLabel->setText(QStringLiteral("Нет подключения"));
    m_videoLabel->setPixmap(QPixmap());
}
