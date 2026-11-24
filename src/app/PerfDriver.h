// Copyright (C) 2026  ScanTailor Advanced contributors
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#ifndef SCANTAILOR_APP_PERFDRIVER_H_
#define SCANTAILOR_APP_PERFDRIVER_H_

class MainWindow;

/**
 * \brief Developer-only scripted driver for repeatable performance measurements.
 *
 * Inert unless the SCANTAILOR_PERF_SCRIPT environment variable is set. The script is a
 * semicolon-separated list of commands:
 *
 *   new=<imageDir>,<outDir>   create a project from all images in a directory
 *   open=<projectFile>        open an existing project
 *   save=<projectFile>        save the current project
 *   threads=<n>               set the batch processing thread count
 *   stage=<n>                 select a processing stage (0-based)
 *   batch                     batch-process all pages at the current stage and wait
 *   settle                    wait until the GUI thread has been responsive for a while
 *   minimize                  minimise the main window (to test background throttling)
 *   wait=<ms>                 wait
 *   quit                      exit without saving
 *
 * One CSV line per command is appended to SCANTAILOR_PERF_OUT (or written to stderr):
 *   command,argument,wall_ms,cpu_ms,max_gui_lag_ms,gui_stall_ms,pages,incomplete_thumbnails
 */
class PerfDriver {
 public:
  /** \return true if a driver was installed and now controls the main window. */
  static bool installIfRequested(MainWindow* mainWindow);
};

#endif  // SCANTAILOR_APP_PERFDRIVER_H_
