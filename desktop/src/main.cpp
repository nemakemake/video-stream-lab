#include <QApplication>
#include <QNetworkProxy>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // без этого системный прокси (например, от VPN) иногда рвёт TCP-соединение с ESP32
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);

    MainWindow window;
    window.show();

    return app.exec();
}
