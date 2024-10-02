#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include "stm32_usb_dev.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qmlRegisterType<stm32_usb_dev>("usbdev", 1, 0, "UsbDev");
    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("USB_GUI", "Main");

    return app.exec();
}
