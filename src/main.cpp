#include "backend.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);
  QQuickStyle::setStyle("Material");

  Backend backend;
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("backend", &backend);

  // To handle failure to load
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

  engine.loadFromModule("MyQtApp", "Main");

  return app.exec();
}
