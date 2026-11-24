// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "Utils.h"

#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QTextDocument>
#include <cmath>

#include "ApplicationSettings.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <stdio.h>
#endif

namespace core {
bool Utils::overwritingRename(const QString& from, const QString& to) {
#ifdef Q_OS_WIN
  return MoveFileExW((WCHAR*) from.utf16(), (WCHAR*) to.utf16(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
  return rename(QFile::encodeName(from).data(), QFile::encodeName(to).data()) == 0;
#endif
}

QString Utils::richTextForLink(const QString& label, const QString& target) {
  return QString::fromLatin1(
             "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\""
             "\"http://www.w3.org/TR/REC-html40/strict.dtd\">"
             "<html><head><meta name=\"qrichtext\" content=\"1\" />"
             "</head><body><p style=\"margin-top:0px; margin-bottom:0px;"
             "margin-left:0px; margin-right:0px; -qt-block-indent:0;"
             "text-indent:0px;\"><a href=\"%1\">%2</a></p></body></html>")
      .arg(target.toHtmlEscaped(), label.toHtmlEscaped());
}

void Utils::maybeCreateCacheDir(const QString& outputDir) {
  QDir(outputDir).mkdir(QString::fromLatin1("cache"));

  // QDir::mkdir() returns false if the directory already exists,
  // so to prevent confusion this function return void.
}

namespace {
QString projectThumbDir(const QString& outputDir) {
  return outputDir + QLatin1String("/cache/thumbs");
}

/**
 * Whether the directory is managed by a cloud sync client that can replace local
 * files with placeholders (macOS File Provider: OneDrive, Dropbox, Google Drive,
 * iCloud Drive). SCANTAILOR_THUMB_CACHE=local|project overrides the detection.
 */
bool useLocalThumbCache(const QString& outputDir) {
  const QByteArray override(qgetenv("SCANTAILOR_THUMB_CACHE"));
  if (override == "local") {
    return true;
  } else if (override == "project") {
    return false;
  }
#ifdef Q_OS_MACOS
  const QString path(QDir::cleanPath(QDir(outputDir).absolutePath()) + QLatin1Char('/'));
  const QString library(QDir::homePath() + QLatin1String("/Library/"));
  return path.startsWith(library + QLatin1String("CloudStorage/"))
         || path.startsWith(library + QLatin1String("Mobile Documents/"));
#else
  Q_UNUSED(outputDir);
  return false;
#endif
}
}  // namespace

QString Utils::outputDirToThumbDir(const QString& outputDir) {
  if (!useLocalThumbCache(outputDir)) {
    return projectThumbDir(outputDir);
  }

  const QByteArray key(QDir::cleanPath(QDir(outputDir).absolutePath()).toUtf8());
  const QString dirName(QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(16)));
  const QString thumbDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QLatin1String("/thumbs/")
                         + dirName);
  QDir().mkpath(thumbDir);
  return thumbDir;
}

QString Utils::outputDirToFallbackThumbDir(const QString& outputDir) {
  return useLocalThumbCache(outputDir) ? projectThumbDir(outputDir) : QString();
}

std::shared_ptr<ThumbnailPixmapCache> Utils::createThumbnailCache(const QString& outputDir) {
  const QSize maxPixmapSize = ApplicationSettings::getInstance().getThumbnailQuality();
  auto cache = std::make_shared<ThumbnailPixmapCache>(outputDirToThumbDir(outputDir), maxPixmapSize, 40, 5);
  cache->setFallbackThumbDir(outputDirToFallbackThumbDir(outputDir));
  return cache;
}

QString Utils::qssConvertPxToEm(const QString& stylesheet, const double base, const int precision) {
  QString result = "";
  const QRegularExpression pxToEm(R"((\d+(\.\d+)?)px)");

  QRegularExpressionMatchIterator iter = pxToEm.globalMatch(stylesheet);
  int prevIndex = 0;
  while (iter.hasNext()) {
    QRegularExpressionMatch match = iter.next();
    result.append(stylesheet.mid(prevIndex, match.capturedStart() - prevIndex));

    double value = match.captured(1).toDouble();
    value /= base;
    result.append(QString::number(value, 'f', precision)).append("em");

    prevIndex = match.capturedEnd();
  }
  result.append(stylesheet.mid(prevIndex));
  return result;
}
}  // namespace core
