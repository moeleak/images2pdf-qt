#include "backend.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

int main(int argc, char *argv[]) {
  qputenv("QT_SCALE_FACTOR", "0.75");
  QGuiApplication app(argc, argv);
  QQuickStyle::setStyle("Material");

  qmlRegisterUncreatableType<Backend>("Images2Pdf", 1, 0, "Backend",
                                      "Create backend for UI access");

  Backend backend;
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("backend", &backend);

  // To handle failure to load
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

  engine.loadFromModule("images2pdf_qt", "Main");

  return app.exec();
}
