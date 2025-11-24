#include "backend.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QScopeGuard>
#include <QSet>
#include <QtConcurrent>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

ImageModel::ImageModel(QObject *parent) : QAbstractListModel(parent) {}

int ImageModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_data.size();
}

QVariant ImageModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_data.size())
    return QVariant();

  if (role == PathRole || role == Qt::DisplayRole) {
    return m_data.at(index.row());
  }
  return QVariant();
}

QHash<int, QByteArray> ImageModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[PathRole] = "modelData";
  return roles;
}

void ImageModel::addPaths(const QStringList &paths) {
  if (paths.isEmpty())
    return;
  beginInsertRows(QModelIndex(), m_data.size(),
                  m_data.size() + paths.size() - 1);
  m_data.append(paths);
  endInsertRows();
}

void ImageModel::removeAt(int index) {
  if (index < 0 || index >= m_data.size())
    return;
  beginRemoveRows(QModelIndex(), index, index);
  m_data.removeAt(index);
  endRemoveRows();
}

void ImageModel::move(int from, int to) {
  if (from < 0 || from >= m_data.size() || to < 0 || to >= m_data.size() ||
      from == to)
    return;

  int dest = (to > from) ? to + 1 : to;

  if (beginMoveRows(QModelIndex(), from, from, QModelIndex(), dest)) {
    m_data.move(from, to);
    endMoveRows();
  }
}

void ImageModel::clear() {
  if (m_data.isEmpty())
    return;
  beginResetModel();
  m_data.clear();
  endResetModel();
}

void ImageModel::replaceAll(const QStringList &paths) {
  if (m_data == paths)
    return;
  beginResetModel();
  m_data = paths;
  endResetModel();
}

const QStringList &ImageModel::getList() const { return m_data; }

int ImageModel::count() const { return m_data.size(); }

namespace {
const QSet<QString> &supportedImageExtensions() {
  static const QSet<QString> extensions = {
      QStringLiteral("png"),  QStringLiteral("jpg"),  QStringLiteral("jpeg"),
      QStringLiteral("bmp"),  QStringLiteral("gif"),  QStringLiteral("webp"),
      QStringLiteral("tif"),  QStringLiteral("tiff"), QStringLiteral("jfif"),
      QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("avif")};
  return extensions;
}

bool hasSupportedExtension(const QString &filePath) {
  const QString extension = QFileInfo(filePath).suffix().toLower();
  return supportedImageExtensions().contains(extension);
}
} // namespace

Backend::Backend(QObject *parent)
    : QObject(parent), m_windowTitle(QStringLiteral("批量图片转 PDF")),
      m_statusText(QStringLiteral("请选择需要转换的图片。")),
      m_conversionRunning(false), m_conversionProgress(0.0),
      m_sortMode(SortNameAscending) {
  m_model = new ImageModel(this);
  connect(&m_scanWatcher, &QFutureWatcher<QStringList>::finished, this,
          &Backend::handleDirectoryScanFinished);
}

QObject *Backend::imageModel() const { return m_model; }
int Backend::imageCount() const { return m_model->count(); }

QString Backend::windowTitle() const { return m_windowTitle; }
QString Backend::statusText() const { return m_statusText; }
bool Backend::conversionRunning() const { return m_conversionRunning; }
double Backend::conversionProgress() const { return m_conversionProgress; }
int Backend::sortMode() const { return static_cast<int>(m_sortMode); }

void Backend::setSortMode(int mode) {
  const SortMode normalized = normalizeSortMode(mode);
  if (m_sortMode == normalized) {
    if (normalized == SortManual) {
      setStatusText(QStringLiteral("当前为手动排序，可使用箭头调整顺序。"));
    } else {
      applyCurrentSort(true);
    }
    return;
  }

  m_sortMode = normalized;
  emit sortModeChanged();

  if (normalized == SortManual) {
    setStatusText(QStringLiteral("已切换到手动排序，可使用箭头调整顺序。"));
    return;
  }

  applyCurrentSort(true);
}

void Backend::addImages(const QStringList &paths) {
  QStringList normalized;
  normalized.reserve(paths.size());

  const QStringList &currentFiles = m_model->getList();

  for (const QString &path : paths) {
    const QString cleaned = cleanedPath(path);
    if (cleaned.isEmpty() || currentFiles.contains(cleaned)) {
      continue;
    }
    normalized.append(cleaned);
  }

  if (normalized.isEmpty()) {
    setStatusText(QStringLiteral("没有新的图片被添加。"));
    return;
  }

  m_model->addPaths(normalized);
  applyCurrentSort(false);
  emit imageCountChanged();
  setStatusText(tr("已选择 %1 张图片。").arg(m_model->count()));
}

void Backend::removeImage(int index) {
  m_model->removeAt(index);
  emit imageCountChanged();
  setStatusText(tr("剩余 %1 张图片。").arg(m_model->count()));
}

void Backend::moveImage(int fromIndex, int toIndex) {
  m_model->move(fromIndex, toIndex);
  setStatusText(QStringLiteral("已更新图片顺序。"));
}

void Backend::clearImages() {
  m_model->clear();
  emit imageCountChanged();
  setStatusText(QStringLiteral("已清空所有图片。"));
}

bool Backend::addDirectory(const QString &directoryPath,
                           bool includeSubdirectories) {
  if (m_scanWatcher.isRunning()) {
    setStatusText(QStringLiteral("正在读取文件夹，请稍候…"));
    return false;
  }

  QString input = directoryPath.trimmed();
  if (input.isEmpty()) {
    setStatusText(QStringLiteral("请选择有效的文件夹。"));
    return false;
  }

  const QUrl directoryUrl = QUrl::fromUserInput(input);
  if (directoryUrl.isValid() && !directoryUrl.scheme().isEmpty()) {
    if (directoryUrl.scheme().compare(QLatin1String("file"),
                                      Qt::CaseInsensitive) == 0) {
      input = directoryUrl.toLocalFile();
    } else {
      setStatusText(QStringLiteral("只支持本地文件夹。"));
      return false;
    }
  }

  QDir dir(input);
  if (!dir.exists()) {
    setStatusText(QStringLiteral("文件夹不存在。"));
    return false;
  }

  setStatusText(QStringLiteral("正在扫描文件夹…"));

  const QString targetPath = dir.absolutePath();
  auto future = QtConcurrent::run([targetPath, includeSubdirectories]() {
    QStringList foundFiles;
    QSet<QString> seen;
    const QDirIterator::IteratorFlags flags =
        includeSubdirectories ? QDirIterator::Subdirectories
                              : QDirIterator::NoIteratorFlags;
    QDirIterator it(targetPath, QDir::Files, flags);
    while (it.hasNext()) {
      const QString filePath = QDir::cleanPath(it.next());
      if (!hasSupportedExtension(filePath))
        continue;
      if (seen.contains(filePath))
        continue;
      seen.insert(filePath);
      foundFiles.append(filePath);
    }
    return foundFiles;
  });
  m_scanWatcher.setFuture(future);
  return true;
}

void Backend::handleDirectoryScanFinished() {
  const QStringList files = m_scanWatcher.result();
  if (files.isEmpty()) {
    setStatusText(QStringLiteral("该文件夹中没有可用的图片。"));
    return;
  }
  addImages(files);
}

bool Backend::convertToPdf(const QString &outputFile, int marginMillimeters,
                           bool stretchToPage, const QString &pageSizeId,
                           bool landscapeOrientation, bool convertToGrayscale) {
  if (m_conversionRunning) {
    setStatusText(QStringLiteral("正在转换，请稍候…"));
    return false;
  }

  if (m_model->count() == 0) {
    setStatusText(QStringLiteral("请先添加至少一张图片。"));
    return false;
  }

  QString trimmed = outputFile.trimmed();
  if (trimmed.isEmpty()) {
    setStatusText(QStringLiteral("请选择输出 PDF 文件。"));
    return false;
  }

  const QUrl outputUrl = QUrl::fromUserInput(trimmed);
  if (outputUrl.isValid() && !outputUrl.scheme().isEmpty()) {
    if (outputUrl.scheme().compare(QLatin1String("file"),
                                   Qt::CaseInsensitive) == 0) {
      trimmed = outputUrl.toLocalFile();
    } else {
      setStatusText(QStringLiteral("只支持保存到本地文件。"));
      return false;
    }
  }

  QFileInfo outputInfo(trimmed);
  if (!outputInfo.dir().exists()) {
    if (!QDir().mkpath(outputInfo.dir().absolutePath())) {
      setStatusText(QStringLiteral("无法创建输出目录。"));
      return false;
    }
  }

  QPdfWriter writer(outputInfo.absoluteFilePath());
  writer.setResolution(300);

  marginMillimeters = std::clamp(marginMillimeters, 0, 50);
  const QPageSize pageSize = pageSizeFromName(pageSizeId);
  QPageLayout baseLayout(pageSize,
                         landscapeOrientation ? QPageLayout::Landscape
                                              : QPageLayout::Portrait,
                         QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
  writer.setPageLayout(baseLayout);

  QPainter painter(&writer);
  if (!painter.isActive()) {
    setStatusText(QStringLiteral("无法创建 PDF 文件。"));
    return false;
  }

  const QSize pagePixels =
      baseLayout.fullRectPixels(writer.resolution()).size();
  const double pixelsPerMillimeter = writer.resolution() / 25.4;
  const int marginPixels = std::clamp(
      static_cast<int>(std::round(marginMillimeters * pixelsPerMillimeter)), 0,
      std::numeric_limits<int>::max());
  const int usableWidth = pagePixels.width() - marginPixels * 2;
  const int usableHeight = pagePixels.height() - marginPixels * 2;
  if (usableWidth <= 0 || usableHeight <= 0) {
    setStatusText(QStringLiteral("边距过大，无法绘制内容。"));
    return false;
  }
  const QRect pageRect(marginPixels, marginPixels, usableWidth, usableHeight);

  setConversionRunning(true);
  setConversionProgress(0.0);
  const auto guard = qScopeGuard([this]() {
    setConversionRunning(false);
    setConversionProgress(0.0);
  });

  int convertedPages = 0;
  QStringList failedFiles;

  const QStringList &fileList = m_model->getList();
  const int totalFiles = fileList.size();

  for (int i = 0; i < fileList.size(); ++i) {
    const QString &path = fileList.at(i);
    const QString fileName = QFileInfo(path).fileName();
    setStatusText(tr("正在处理第 %1/%2 张：%3")
                      .arg(i + 1)
                      .arg(std::max(1, totalFiles))
                      .arg(fileName));
    QImage image(path);
    if (image.isNull()) {
      failedFiles << fileName;
      continue;
    }

    if (convertToGrayscale) {
      image = image.convertToFormat(QImage::Format_Grayscale8);
    }

    if (convertedPages > 0) {
      if (!writer.newPage()) {
        setStatusText(QStringLiteral("无法创建 PDF 页面。"));
        return false;
      }
      writer.setPageLayout(baseLayout);
    }

    QRect targetRect = pageRect;

    if (!stretchToPage) {
      QSize size = image.size();
      size.scale(pageRect.size(), Qt::KeepAspectRatio);
      const QPoint offset(pageRect.x() + (pageRect.width() - size.width()) / 2,
                          pageRect.y() +
                              (pageRect.height() - size.height()) / 2);
      targetRect = QRect(offset, size);
    }

    painter.drawImage(targetRect, image);
    ++convertedPages;
    setConversionProgress(static_cast<double>(i + 1) / std::max(1, totalFiles));
    QCoreApplication::processEvents();
  }

  if (convertedPages == 0) {
    setStatusText(QStringLiteral("没有任何图片被写入。"));
    return false;
  }

  if (!failedFiles.isEmpty()) {
    setStatusText(tr("转换完成，但跳过了 %1 个文件：%2")
                      .arg(failedFiles.size())
                      .arg(failedFiles.join(", ")));
  } else {
    setStatusText(tr("成功将 %1 张图片保存到 %2")
                      .arg(convertedPages)
                      .arg(outputInfo.fileName()));
  }

  setConversionProgress(1.0);
  return true;
}

void Backend::setStatusText(const QString &text) {
  if (m_statusText == text) {
    return;
  }
  m_statusText = text;
  emit statusTextChanged();
}

void Backend::setConversionRunning(bool running) {
  if (m_conversionRunning == running) {
    return;
  }
  m_conversionRunning = running;
  emit conversionRunningChanged();
}

void Backend::setConversionProgress(double progress) {
  const double clamped = std::clamp(progress, 0.0, 1.0);
  if (std::abs(clamped - m_conversionProgress) < 0.00001) {
    return;
  }
  m_conversionProgress = clamped;
  emit conversionProgressChanged();
}

QString Backend::cleanedPath(const QString &path) const {
  QString input = path.trimmed();
  if (input.isEmpty()) {
    return QString();
  }

  const QUrl url = QUrl::fromUserInput(input);
  if (url.isValid() && !url.scheme().isEmpty()) {
    if (url.scheme().compare(QLatin1String("file"), Qt::CaseInsensitive) == 0) {
      input = url.toLocalFile();
    } else {
      return QString();
    }
  }

  const QFileInfo info(input);
  if (!info.exists() || !info.isFile()) {
    return QString();
  }

  return QDir::cleanPath(info.absoluteFilePath());
}

QPageSize Backend::pageSizeFromName(const QString &pageName) const {
  const QString key = pageName.trimmed().toUpper();
  if (key == QLatin1String("A3")) {
    return QPageSize(QPageSize::A3);
  }
  if (key == QLatin1String("A5")) {
    return QPageSize(QPageSize::A5);
  }
  if (key == QLatin1String("LETTER")) {
    return QPageSize(QPageSize::Letter);
  }
  if (key == QLatin1String("LEGAL")) {
    return QPageSize(QPageSize::Legal);
  }
  if (key == QLatin1String("B5")) {
    return QPageSize(QPageSize::B5);
  }
  if (key == QLatin1String("TABLOID")) {
    return QPageSize(QPageSize::Tabloid);
  }
  return QPageSize(QPageSize::A4);
}

void Backend::applyCurrentSort(bool announceChange) {
  const SortMode mode = m_sortMode;
  if (mode == SortManual) {
    if (announceChange) {
      setStatusText(QStringLiteral("当前为手动排序，可使用箭头调整顺序。"));
    }
    return;
  }

  const QStringList current = m_model->getList();
  if (current.size() < 2) {
    if (announceChange) {
      setStatusText(sortDescription(mode));
    }
    return;
  }

  QStringList sorted = current;

  switch (mode) {
  case SortNameAscending:
    resortByName(sorted, true);
    break;
  case SortNameDescending:
    resortByName(sorted, false);
    break;
  case SortTimeNewestFirst:
    resortByTime(sorted, true);
    break;
  case SortTimeOldestFirst:
    resortByTime(sorted, false);
    break;
  case SortManual:
    break;
  }

  if (sorted != current) {
    m_model->replaceAll(sorted);
  }

  if (announceChange) {
    setStatusText(sortDescription(mode));
  }
}

Backend::SortMode Backend::normalizeSortMode(int value) {
  switch (static_cast<SortMode>(value)) {
  case SortManual:
  case SortNameAscending:
  case SortNameDescending:
  case SortTimeNewestFirst:
  case SortTimeOldestFirst:
    return static_cast<SortMode>(value);
  }
  return SortManual;
}

void Backend::resortByName(QStringList &entries, bool ascending) const {
  if (entries.size() < 2)
    return;

  struct NameEntry {
    QString path;
    QString fileName;
  };

  std::vector<NameEntry> data;
  data.reserve(entries.size());
  for (const QString &path : entries) {
    data.push_back(NameEntry{path, QFileInfo(path).fileName()});
  }

  QCollator collator;
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(true);

  std::stable_sort(data.begin(), data.end(),
                   [ascending, collator](const NameEntry &left,
                                         const NameEntry &right) mutable {
                     int cmp = collator.compare(left.fileName, right.fileName);
                     if (cmp == 0) {
                       cmp = collator.compare(left.path, right.path);
                     }
                     if (cmp == 0)
                       return false;
                     return ascending ? cmp < 0 : cmp > 0;
                   });

  for (int i = 0; i < static_cast<int>(data.size()); ++i) {
    entries[i] = data.at(i).path;
  }
}

void Backend::resortByTime(QStringList &entries, bool newestFirst) const {
  if (entries.size() < 2)
    return;

  struct TimeEntry {
    QString path;
    qint64 timestamp;
  };

  std::vector<TimeEntry> data;
  data.reserve(entries.size());
  for (const QString &path : entries) {
    const QFileInfo info(path);
    const QDateTime modified = info.lastModified();
    const qint64 stamp = modified.isValid() ? modified.toMSecsSinceEpoch() : 0;
    data.push_back(TimeEntry{path, stamp});
  }

  std::stable_sort(
      data.begin(), data.end(),
      [newestFirst](const TimeEntry &left, const TimeEntry &right) {
        if (left.timestamp == right.timestamp) {
          return left.path < right.path;
        }
        return newestFirst ? left.timestamp > right.timestamp
                           : left.timestamp < right.timestamp;
      });

  for (int i = 0; i < static_cast<int>(data.size()); ++i) {
    entries[i] = data.at(i).path;
  }
}

QString Backend::sortDescription(SortMode mode) const {
  switch (mode) {
  case SortNameAscending:
    return tr("已按文件名排序（A → Z）。");
  case SortNameDescending:
    return tr("已按文件名排序（Z → A）。");
  case SortTimeNewestFirst:
    return tr("已按修改时间排序（最新在前）。");
  case SortTimeOldestFirst:
    return tr("已按修改时间排序（最旧在前）。");
  case SortManual:
    break;
  }
  return QString();
}
