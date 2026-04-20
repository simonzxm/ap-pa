#include "gamecontroller.h"

#include <QGuiApplication>
#include <QCoreApplication>
#include <QObject>
#include <QUrl>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    GameController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("game", &controller);

    engine.loadFromModule("Synera", "Main");
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    return app.exec();
}
