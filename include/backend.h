#ifndef BACKEND_H
#define BACKEND_H

#include <QAbstractListModel>
#include <QObject>
#include <QPageSize>
#include <QString>
#include <QStringList>
#include <QFutureWatcher>

class ImageModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum Roles { PathRole = Qt::UserRole + 1 };

  explicit ImageModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void addPaths(const QStringList &paths);
  void removeAt(int index);
  void move(int from, int to);
  void clear();
  void replaceAll(const QStringList &paths);

  const QStringList &getList() const;
  int count() const;

private:
  QStringList m_data;
};

class Backend : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *imageModel READ imageModel CONSTANT)
  Q_PROPERTY(int imageCount READ imageCount NOTIFY imageCountChanged)

  Q_PROPERTY(QString windowTitle READ windowTitle CONSTANT)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
  Q_PROPERTY(bool conversionRunning READ conversionRunning NOTIFY
                 conversionRunningChanged)
  Q_PROPERTY(double conversionProgress READ conversionProgress NOTIFY
                 conversionProgressChanged)
  Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY
                 sortModeChanged)

public:
  enum SortMode {
    SortManual = 0,
    SortNameAscending,
    SortNameDescending,
    SortTimeNewestFirst,
    SortTimeOldestFirst
  };
  Q_ENUM(SortMode)

  explicit Backend(QObject *parent = nullptr);

  QObject *imageModel() const;
  int imageCount() const;

  QString windowTitle() const;
  QString statusText() const;
  bool conversionRunning() const;
  double conversionProgress() const;
  int sortMode() const;
  void setSortMode(int mode);

  Q_INVOKABLE void addImages(const QStringList &paths);
  Q_INVOKABLE bool addDirectory(const QString &directoryPath,
                                bool includeSubdirectories = true);
  Q_INVOKABLE void removeImage(int index);
  Q_INVOKABLE void moveImage(int fromIndex, int toIndex);
  Q_INVOKABLE void clearImages();
  Q_INVOKABLE bool
  convertToPdf(const QString &outputFile, int marginMillimeters = 10,
               bool stretchToPage = false,
               const QString &pageSizeId = QStringLiteral("A4"),
               bool landscapeOrientation = false,
               bool convertToGrayscale = false);

signals:
  void statusTextChanged();
  void imageCountChanged(); // 替代原来的 imageFilesChanged
  void conversionRunningChanged();
  void conversionProgressChanged();
  void sortModeChanged();

private:
  void setStatusText(const QString &text);
  void setConversionRunning(bool running);
  void setConversionProgress(double progress);
  QString cleanedPath(const QString &path) const;
  QPageSize pageSizeFromName(const QString &pageName) const;
  void applyCurrentSort(bool announceChange);
  static SortMode normalizeSortMode(int value);
  void resortByName(QStringList &entries, bool ascending) const;
  void resortByTime(QStringList &entries, bool newestFirst) const;
  QString sortDescription(SortMode mode) const;
  void handleDirectoryScanFinished();

  QString m_windowTitle;
  QString m_statusText;
  bool m_conversionRunning;
  double m_conversionProgress;
  SortMode m_sortMode;

  ImageModel *m_model;
  QFutureWatcher<QStringList> m_scanWatcher;
};

#endif // BACKEND_H
