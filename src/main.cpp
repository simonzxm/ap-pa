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

    const QUrl url(QStringLiteral("qrc:/Synera/qml/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(url);
    return app.exec();
}
