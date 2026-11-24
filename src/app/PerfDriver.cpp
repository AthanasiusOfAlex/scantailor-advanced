// Copyright (C) 2026  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include "PerfDriver.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QSettings>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#if defined(Q_OS_UNIX)
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "Dpi.h"
#include "ImageFileInfo.h"
#include "ImageMetadata.h"
#include "ImageMetadataLoader.h"
#include "IncompleteThumbnail.h"
#include "MainWindow.h"
#include "PageSequence.h"
#include "ProjectPages.h"

namespace {
qint64 processCpuMs() {
#if defined(Q_OS_UNIX)
  struct rusage usage {};
  getrusage(RUSAGE_SELF, &usage);
  const qint64 user = qint64(usage.ru_utime.tv_sec) * 1000 + usage.ru_utime.tv_usec / 1000;
  const qint64 sys = qint64(usage.ru_stime.tv_sec) * 1000 + usage.ru_stime.tv_usec / 1000;
  return user + sys;
#else
  return 0;
#endif
}

[[noreturn]] void hardExit(const int code) {
  std::fflush(nullptr);
#if defined(Q_OS_UNIX)
  _exit(code);
#else
  std::_Exit(code);
#endif
}
}  // namespace

// Not a QObject on purpose: a single self-contained file is easy to drop into older
// revisions when bisecting. MainWindow declares it a friend.
class PerfDriverImpl {
 public:
  PerfDriverImpl(MainWindow* wnd, const QString& script) : m_wnd(wnd), m_commands(script.split(';', Qt::SkipEmptyParts)) {
    const QString outPath = qEnvironmentVariable("SCANTAILOR_PERF_OUT");
    if (!outPath.isEmpty()) {
      m_outFile.setFileName(outPath);
      m_outFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }

    bool ok = false;
    const int timeoutSec = qEnvironmentVariableIntValue("SCANTAILOR_PERF_TIMEOUT", &ok);
    QTimer::singleShot((ok ? timeoutSec : 3600) * 1000, &m_context, [this] {
      emitLine("TIMEOUT", m_currentCommand, 0, 0, 0);
      hardExit(4);
    });

    // Heartbeat: measures how long the GUI thread goes without servicing its event loop,
    // and aborts if a modal dialog would block the script.
    m_heartbeat.setInterval(HEARTBEAT_MS);
    m_heartbeat.setTimerType(Qt::PreciseTimer);
    QObject::connect(&m_heartbeat, &QTimer::timeout, &m_context, [this] { onHeartbeat(); });
    m_heartbeatClock.start();
    m_heartbeat.start();

    QTimer::singleShot(0, &m_context, [this] { next(); });
  }

 private:
  static constexpr int HEARTBEAT_MS = 10;
  static constexpr int STALL_THRESHOLD_MS = 50;

  void onHeartbeat() {
    const qint64 gap = m_heartbeatClock.restart();
    const qint64 lag = std::max<qint64>(0, gap - HEARTBEAT_MS);
    m_maxLagMs = std::max(m_maxLagMs, lag);
    if (gap > STALL_THRESHOLD_MS) {
      m_stallMs += gap;
    }
    m_quietMs = (gap > STALL_THRESHOLD_MS) ? 0 : (m_quietMs + gap);

    if (QWidget* modal = QApplication::activeModalWidget()) {
      emitLine("MODAL", QString::fromLatin1(modal->metaObject()->className()) + ":" + modal->windowTitle(), 0, 0, 0);
      hardExit(3);
    }
  }

  void resetLag() {
    m_maxLagMs = 0;
    m_stallMs = 0;
  }

  void emitLine(const QString& cmd, const QString& arg, qint64 wallMs, qint64 cpuMs, qint64 pages) {
    const QString line = QString("%1,%2,%3,%4,%5,%6,%7,%8\n")
                             .arg(cmd, QString(arg).replace(',', '|'))
                             .arg(wallMs)
                             .arg(cpuMs)
                             .arg(m_maxLagMs)
                             .arg(m_stallMs)
                             .arg(pages)
                             .arg(incompleteThumbnails());
    if (m_outFile.isOpen()) {
      m_outFile.write(line.toUtf8());
      m_outFile.flush();
    }
    std::fputs(("PERF " + line).toUtf8().constData(), stderr);
  }

  /** Pages the current stage considers unprocessed (shown with a question mark). */
  int incompleteThumbnails() const {
    int count = 0;
    if (QGraphicsScene* scene = m_wnd->thumbView->scene()) {
      for (QGraphicsItem* item : scene->items()) {
        if (dynamic_cast<IncompleteThumbnail*>(item)) {
          ++count;
        }
      }
    }
    return count;
  }

  qint64 pageCount() const { return m_wnd->isProjectLoaded() ? qint64(m_wnd->allPages().numPages()) : 0; }

  void fail(const QString& what) {
    emitLine("ERROR", what, 0, 0, 0);
    hardExit(2);
  }

  void next() {
    if (m_commands.isEmpty()) {
      return;
    }
    m_currentCommand = m_commands.takeFirst().trimmed();
    const int eq = m_currentCommand.indexOf('=');
    const QString cmd = (eq < 0) ? m_currentCommand : m_currentCommand.left(eq);
    const QString arg = (eq < 0) ? QString() : m_currentCommand.mid(eq + 1);

    resetLag();
    QElapsedTimer wall;
    wall.start();
    const qint64 cpu0 = processCpuMs();

    // Synchronous commands report the time the GUI thread was blocked in the call.
    auto finishSync = [&] {
      const qint64 wallMs = wall.elapsed();
      const qint64 cpuMs = processCpuMs() - cpu0;
      m_maxLagMs = std::max(m_maxLagMs, wallMs);
      emitLine(cmd, arg, wallMs, cpuMs, pageCount());
      QTimer::singleShot(0, &m_context, [this] { next(); });
    };

    if (cmd == "new") {
      const QStringList parts = arg.split(',');
      if (parts.size() != 2) {
        fail("new=<imageDir>,<outDir>");
      }
      newProject(parts[0], parts[1]);
      finishSync();
    } else if (cmd == "open") {
      m_wnd->openProject(arg);
      if (!m_wnd->isProjectLoaded()) {
        fail("project did not load: " + arg);
      }
      finishSync();
    } else if (cmd == "save") {
      if (!m_wnd->saveProjectWithFeedback(arg)) {
        fail("save failed: " + arg);
      }
      finishSync();
    } else if (cmd == "threads") {
      QSettings().setValue("settings/batch_processing_threads", arg.toInt());
      finishSync();
    } else if (cmd == "stage") {
      m_wnd->filterList->selectRow(arg.toInt());
      finishSync();
    } else if (cmd == "batch") {
      m_wnd->goFirstPage();
      resetLag();
      wall.restart();
      const qint64 batchCpu0 = processCpuMs();
      m_wnd->startBatchProcessing();
      pollBatch(wall, batchCpu0);
    } else if (cmd == "settle") {
      m_quietMs = 0;
      pollSettle(wall, cpu0);
    } else if (cmd == "minimize") {
      m_wnd->showMinimized();
      finishSync();
    } else if (cmd == "wait") {
      QTimer::singleShot(arg.toInt(), &m_context, [this, cmd, arg, wall, cpu0] {
        emitLine(cmd, arg, wall.elapsed(), processCpuMs() - cpu0, pageCount());
        next();
      });
    } else if (cmd == "quit") {
      emitLine(cmd, arg, 0, 0, pageCount());
      hardExit(0);
    } else {
      fail("unknown command: " + m_currentCommand);
    }
  }

  void pollBatch(const QElapsedTimer wall, const qint64 cpu0) {
    if (m_wnd->isBatchProcessingInProgress()) {
      QTimer::singleShot(5, &m_context, [this, wall, cpu0] { pollBatch(wall, cpu0); });
      return;
    }
    emitLine("batch", QString::number(m_wnd->m_curFilter), wall.elapsed(), processCpuMs() - cpu0, pageCount());
    QTimer::singleShot(0, &m_context, [this] { next(); });
  }

  void pollSettle(const QElapsedTimer wall, const qint64 cpu0) {
    static constexpr int QUIET_MS = 500;
    if (m_quietMs < QUIET_MS) {
      QTimer::singleShot(20, &m_context, [this, wall, cpu0] { pollSettle(wall, cpu0); });
      return;
    }
    emitLine("settle", QString(), wall.elapsed() - QUIET_MS, processCpuMs() - cpu0, pageCount());
    QTimer::singleShot(0, &m_context, [this] { next(); });
  }

  void newProject(const QString& imageDir, const QString& outDir) {
    const QDir dir(imageDir);
    const QStringList names = dir.entryList({"*.tif", "*.tiff", "*.png", "*.jpg", "*.jpeg"}, QDir::Files, QDir::Name);
    if (names.isEmpty()) {
      fail("no images in " + imageDir);
    }

    std::vector<ImageFileInfo> files;
    for (const QString& name : names) {
      const QFileInfo fileInfo(dir.absoluteFilePath(name));
      std::vector<ImageMetadata> metadata;
      const auto status = ImageMetadataLoader::load(
          fileInfo.absoluteFilePath(), [&metadata](const ImageMetadata& item) { metadata.push_back(item); });
      if (status != ImageMetadataLoader::LOADED) {
        fail("cannot read metadata: " + fileInfo.absoluteFilePath());
      }
      for (ImageMetadata& item : metadata) {
        if (!item.isDpiOK()) {
          item.setDpi(Dpi(300, 300));
        }
      }
      files.emplace_back(fileInfo, metadata);
    }

    QDir().mkpath(outDir);
    auto pages = std::make_shared<ProjectPages>(files, ProjectPages::AUTO_PAGES, Qt::LeftToRight);
    m_wnd->switchToNewProject(pages, QDir(outDir).absolutePath());
  }

  MainWindow* m_wnd;
  QStringList m_commands;
  QString m_currentCommand;
  QObject m_context;
  QTimer m_heartbeat;
  QElapsedTimer m_heartbeatClock;
  QFile m_outFile;
  qint64 m_maxLagMs = 0;
  qint64 m_stallMs = 0;
  qint64 m_quietMs = 0;
};

bool PerfDriver::installIfRequested(MainWindow* mainWindow) {
  const QString script = qEnvironmentVariable("SCANTAILOR_PERF_SCRIPT");
  if (script.isEmpty()) {
    return false;
  }
  new PerfDriverImpl(mainWindow, script);  // Lives for the rest of the process.
  return true;
}
