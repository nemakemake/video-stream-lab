#include <QApplication>
#include <QNetworkProxy>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Явно отключаем прокси для приложения: соединение с ESP32/mock-сервером —
    // это всегда прямое TCP на устройство в локальной сети, и оно не должно
    // зависеть от системных настроек прокси (например, оставшихся от VPN-клиента),
    // которые ломают или замедляют подключение непредсказуемым образом.
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    MainWindow window;
    window.show();

    return app.exec();
}
