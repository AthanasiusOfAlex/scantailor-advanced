# ScanTailor Advanced

ScanTailor is an interactive post-processing tool for scanned pages. You give it raw scans; it rotates,
splits, deskews and crops them, sets margins, and produces clean black-and-white, colour or mixed output
pages, ready to print or to assemble into a PDF or [DjVu](https://en.wikipedia.org/wiki/DjVu) file.
Scanning, OCR and assembling multi-page documents are out of scope.

ScanTailor Advanced combines the ScanTailor Featured, Enhanced and Universal variants and adds many
features and fixes of its own.

## This branch: macOS on Apple Silicon

This is the `macos-arm64` branch of a fork of
[ScanTailor-Advanced/scantailor-advanced](https://github.com/ScanTailor-Advanced/scantailor-advanced).
It is upstream **1.2.1** plus the fixes below, and it is what the Homebrew tap
[AthanasiusOfAlex/scantailor-advanced-osx](https://github.com/AthanasiusOfAlex/scantailor-advanced-osx)
builds: a native ARM64 build with Qt 6.

| Change | Upstream PR |
|---|---|
| **No more batch-processing hangs.** The store behind deviation highlighting was shared between worker threads and the main thread without locking. The freed worker also gets its next page before the GUI updates. | [#193](https://github.com/ScanTailor-Advanced/scantailor-advanced/pull/193) |
| **A page that fails no longer kills the app.** An exception in one page's task used to end the session; now it is logged, the page stays unprocessed, and the batch finishes. | [#194](https://github.com/ScanTailor-Advanced/scantailor-advanced/pull/194) |
| **Old projects open again.** 1.2.0 raised the project-file version without changing the format, so projects saved by 1.0.x and 1.1.x were rejected. Pages from those projects also keep oblique correction off, as before. | [#195](https://github.com/ScanTailor-Advanced/scantailor-advanced/pull/195) |
| **Faster batch runs.** Colour pages load about 40% faster, and data needed only for on-screen display is no longer built for every page in a batch. Re-running a stage over a processed 532-page project takes about a third of the time, with byte-identical output. | [#196](https://github.com/ScanTailor-Advanced/scantailor-advanced/pull/196) |
| **Local thumbnail cache for cloud-synced projects.** For projects in OneDrive, Dropbox, Google Drive or iCloud Drive, thumbnails are kept in `~/Library/Caches/scantailor-advanced`, so reopening a project does not download one thumbnail at a time. `SCANTAILOR_THUMB_CACHE=local` or `project` overrides the detection. | macOS only |
| **Settings outside the app bundle.** An app bundle always uses `~/.config/scantailor-advanced/` instead of a `config` folder next to the executable, so settings survive upgrades and the code signature stays valid. | macOS only |
| **`PerfDriver`**, a developer-only hook (`SCANTAILOR_PERF_SCRIPT`) for scripted timing runs. It is inert unless the variable is set. | not proposed |

> [!TIP]
> If a project lives in a cloud-synced folder and its files show a cloud icon in Finder, every page
> ScanTailor reads is downloaded first, at about a second per file. Before working on the project,
> right-click its folder and choose **Always Keep on This Device**.

## Contents

- [Installing](#installing)
- [Building from source](#building-from-source)
- [Features](#features)
  - [Working with projects](#working-with-projects)
  - [Thumbnails and navigation](#thumbnails-and-navigation)
  - [Fix Orientation](#fix-orientation)
  - [Split Pages](#split-pages)
  - [Deskew](#deskew)
  - [Select Content](#select-content)
  - [Margins](#margins)
  - [Output](#output)
  - [Batch processing](#batch-processing)
- [History and credits](#history-and-credits)
- [License](#license)

## Installing

**macOS (Apple Silicon):** use the Homebrew tap, which builds this branch:

```bash
brew tap AthanasiusOfAlex/scantailor-advanced-osx
```

```bash
brew install scantailor-advanced
```

The tap also has an install script and a bundler that makes a self-contained `.app` and `.dmg`; see
[its README](https://github.com/AthanasiusOfAlex/scantailor-advanced-osx#readme).

**Linux and Windows:** use upstream's
[releases](https://github.com/ScanTailor-Advanced/scantailor-advanced/releases).

- **Linux packages.** Each `v*` tag produces a `.deb` and an AppImage. The AppImage is built on
  Ubuntu 24.04 and needs a matching glibc; on Ubuntu 22.04 and older, use the `.deb` or build from source.
- **Flatpak.** There is no Flathub listing yet ([#105](https://github.com/ScanTailor-Advanced/scantailor-advanced/issues/105)).
  The manifest [`flatpak/org.scantailor.Advanced.json`](flatpak/org.scantailor.Advanced.json) builds
  locally with `flatpak-builder`. Its ID, `org.scantailor.Advanced`, does not replace the old
  `com.github._4lex4.*` Flatpak.
- **Wayland.** Qt 5 builds switch to X11 (`xcb`) on Wayland, because Qt 5 there breaks dialogs and
  painting. Set `SCANTAILOR_NO_XCB_FALLBACK=1` to keep native Wayland. Qt 6 builds are not affected.
- **Windows runtime.** If the program does not start and Windows reports a missing `VCRUNTIME*.dll` or
  `MSVCP*.dll`, install the
  [Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
  that matches the build (x64 or x86). Windows 7 and other old versions are not tested.
- **Large JPEG/PNG files on Windows** need a build with libjpeg-turbo (or libjpeg 9+) and a recent
  libpng, as provided by [scantailor-libs-build](https://github.com/ScanTailor-Advanced/scantailor-libs-build).

## Building from source

The build needs CMake, a C++17 compiler, Boost, libjpeg, libpng, libtiff, zlib, and Qt 6 (preferred) or
Qt 5. Build out of tree. `-DBUILD_TESTS=OFF` skips the unit tests, for example in a minimal chroot.

### macOS

```bash
brew install cmake boost qt libpng libtiff jpeg-turbo
```

```bash
git clone --branch macos-arm64 https://github.com/AthanasiusOfAlex/scantailor-advanced.git
```

```bash
cd scantailor-advanced
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DPORTABLE_VERSION=OFF
cmake --build build -j"$(sysctl -n hw.ncpu)"
```

### Linux (Ubuntu / Debian)

```bash
sudo apt install build-essential cmake \
  qt5-qmake qtbase5-dev libqt5svg5-dev qttools5-dev \
  libboost-test-dev libboost-dev \
  libjpeg-dev libpng-dev libtiff-dev zlib1g-dev
```

```bash
mkdir build && cd build && cmake .. && make -j"$(nproc)"
```

`sudo make install` installs system-wide. `./build-deb.sh` builds
`scantailor-advanced_<version>_<arch>.deb` in the source directory, with icon and menu entry.

### Windows

Native builds: follow [scantailor-libs-build](https://github.com/ScanTailor-Advanced/scantailor-libs-build).

Cross-compiling from Linux uses [MXE](https://mxe.cc/). The one-time setup takes an hour or two:

```bash
git clone https://github.com/mxe/mxe.git ~/mxe && cd ~/mxe && make MXE_TARGETS=x86_64-w64-mingw32.static qt5 jpeg libpng tiff zlib boost
```

Then, from the source directory, `./build-windows.sh` produces `build-win-static/scantailor.exe`.
`./build-windows.sh shared` makes a smaller build that needs the DLLs from MXE's
`usr/x86_64-w64-mingw32.shared/bin/` next to the `.exe`.

Community test material is collected in
[scantailor-testing](https://github.com/ImageProcessing-ElectronicPublications/scantailor-testing).

## Features

ScanTailor works in six stages: Fix Orientation, Split Pages, Deskew, Select Content, Margins and Output.
The features are listed below in that order, after those that apply everywhere.

### Working with projects

- **Relative paths.** Project files store image paths relative to the project file where possible, so
  a project folder can be moved or synced.
- **Auto-save.** With *Auto-save the existing project* enabled in the settings, a project that has
  been saved once is saved automatically, during batch processing too.
- **Default parameters.** *Tools → Default parameters* sets the starting settings of every stage for
  new projects, in named profiles. The built-in profiles are *Default* (ScanTailor's defaults) and
  *Source* (output identical to the input). User profiles are stored, with their units, in
  `config/profiles` next to a portable executable, or in the per-user application-data folder.
  Values are converted when the units differ.
- **Units.** Pixels, millimetres, centimetres or inches (*Tools → Units*), used everywhere, including
  margins and the status bar.
- **Status bar.** Shows the zone creation mode, the cursor position on the image, the physical size of
  the image, the page's position in the current order, and its name and side (`[L]` or `[R]` after
  splitting).
- **Reload.** **`F5`** reloads the current page. Output is regenerated when a source image has changed.
- **Fix DPI and Relinking** (*Tools* menu) correct wrong source resolutions and find images that have
  moved.
- **Interface.** Dark, light or native colour scheme. Option groups can be collapsed, and stay so
  between sessions. Optional OpenGL acceleration. Translations: French, German, Korean, Polish,
  Portuguese, Russian and Spanish.
- **Settings location.** A portable build (`PORTABLE_VERSION`, on by default) keeps its settings in a
  `config` folder next to the executable when that folder is writable, and in the system's per-user
  location otherwise. On macOS an app bundle always uses the per-user location.

### Thumbnails and navigation

- Thumbnails can be shown in one or several columns, and the panel can be undocked.
  **`Ctrl++`** / **`Ctrl+-`** or **`Alt+Wheel`** scale them. The settings set the thumbnail quality and
  display size (defaults 200 and 250 pixels); a separate cache is kept for each quality.
- Pages can be ordered by decreasing deviation, and pages with high deviation can be marked with a red
  asterisk. The deviation is computed per stage (skew at Deskew, content size at Select Content, page
  size at Margins), and the settings set how sensitive it is.
- *Pause automatic thumbnail reordering* keeps the order still while thumbnails refresh.
- Selection: **`Ctrl+Click`**, **`Shift+Click`**, **`Ctrl+drag`** (rubber band), or the multi-page
  selection mode without the keyboard. The selection survives switching stages, and the leader of the
  selection is highlighted separately.
- Navigation: **`Home`**, **`End`**, **`PgUp`/`Q`** and **`PgDown`/`W`** move between pages,
  **`Shift+PgUp/Q`** and **`Shift+PgDown/W`** between selected pages, and **`Ctrl+G`** goes to a page
  number.

### Fix Orientation

- Rotate pages in steps of 90°.
- **Manual trim** crops each scan before the later stages.

### Split Pages

- Applying a cut to pages of slightly different size adapts the cutters instead of switching those pages
  back to auto mode. Invalid manual cuts are rejected, and the cutters cannot cross each other.
- The split is respected in every later view and in the thumbnails. *Fill offcut* (at Output) fills the
  cut-off part of the page.
- *Tools → Reverse two-page spread order* swaps left and right pages, for example for right-to-left
  books.

### Deskew

- Auto or manual rotation. **Top page edge** mode measures the skew from the top edge of the page, which
  works well for book scans on a dark background.
- **Oblique (shear) correction**, automatic or manual, with a vertical lever for adjusting it by hand. It
  can be turned off for pages that only need rotation, and the default can be set in a profile.

### Select Content

- **Page detection** finds the page in black margins, or can be turned off to keep the original layout.
- The **page box** is interactive, like the content box, and has width and height fields in manual mode.
  Changing the content box does not reset it. Applying page or content boxes to other pages corrects
  their position for each page.
- **`Double-click`** on content fits the content box to it: the box grows to take in content outside
  it, or the nearest edge moves. Hold **`Shift`** for the left or right edge only, **`Ctrl`** for the top
  or bottom edge only, **`Shift+Ctrl`** for both nearest edges.
- **`Shift+LMB`** drags the content and page boxes together.

### Margins

- **Alignment.** Horizontal and vertical alignment can each be automatic. *Auto margins* decides between
  aligning a page to the top, centre or bottom; *Original* keeps the page where it was on the scan,
  taking the page box from Select Content into account.
- **Match size with other pages**, optionally applied to every page at once. *Lock aggregate size for
  matching* keeps the reference size fixed while switching pages.
- Margins can be negative, and the source DPI is shown in the panel.
- **Guides** are horizontal or vertical lines for positioning content; they follow the page when its
  margins change.
  - **`Right-click`** adds or removes guides.
  - **`Ctrl+Alt+LMB`** drags a guide.
  - **`Shift+LMB`** or **`Ctrl+LMB`** on the content moves it horizontally or vertically only, and
    **`Shift+Ctrl+LMB`** moves it freely.
  - **`Double-click`** on content attaches it to the nearest guide (**`Shift`** for vertical guides,
    **`Ctrl`** for horizontal ones, **`Shift+Ctrl`** for both).
  - The context menu also shows or hides the hard-margins rectangle.

### Output

- **Modes:** black and white, colour/grayscale, or mixed. Output resolution up to 1200 DPI.
- **Binarization methods:** Otsu, Sauvola, Wolf, Fox, Window, Bradley, Grad, EdgePlus, BlurDiv and EdgeDiv.
  Illumination equalization (before binarization and in colour areas), Savitzky-Golay and morphological
  smoothing can be set in any mode where they apply.
- **Colour operations:** Wiener denoiser, colour segmentation, posterization (2 to 6 levels give an
  indexed image), normalize, and *Force b&w* for grayscale gradients.
- **Filling:** areas outside the content are filled with the estimated background colour, white or black.
  *Fill margins*, *Fill offcut* and *Fill outside page box* choose which areas.
- **Despeckling** has a strength slider: 1.0 matches the old *cautious* setting, 2.0 *normal* and 3.0
  *aggressive*.
- **Light content on a dark background** can be detected automatically (with a separate setting for the
  Output stage) and set per page.
- **Picture zones** in mixed mode: automatic detection with free or rectangular shapes (a sensitivity of
  25% reproduces the old Featured *Quadro* mode, 100% the old Enhanced *Rectangular* one), plus zones
  drawn by hand. A zone's properties make it add to or subtract from the picture layer, or add its area
  to the foreground or background.
  - **`Z`** polygonal, **`X`** lasso, **`C`** rectangular creation mode.
  - **`Shift+LMB`** drags a zone, **`Ctrl+Shift+LMB`** drags a copy, **`Ctrl+Alt+Click`** copies the
    last zone to the cursor, **`Del`** deletes the zone under the cursor.
  - **`D`** deletes the vertex under the cursor; hold **`Ctrl`** while dragging a vertex to make a right
    angle.
- **Split output** writes mixed pages as a foreground layer (black-and-white or colour text) and a
  background layer (pictures), for DjVu encoding or for processing each layer separately. *Original
  background* also saves the untouched background of a black-and-white foreground in
  `out/original_background`: pure black and white become `#010101` and `#fefefe`, picture zones are
  black and text is white, so an image editor can select areas by colour.
- **Dewarping:** manual, automatic, or *marginal*, which follows the curved top and bottom page edges on
  a dark background. Changes to the mesh switch the page to manual mode.
  - **`Ctrl`** + drag on the leftmost or rightmost point of the top or bottom mesh line moves the whole
    line up or down.
- **Tabs.** **`Ctrl+1`** to **`Ctrl+5`** switch the output tabs, and the zoom and position are kept when
  switching.
- **TIFF compression** is set separately for black-and-white output (none, LZW, Deflate, CCITT G4) and
  colour output (none, LZW, Deflate, JPEG).

### Batch processing

Pages are processed by several threads at once; the number can be changed while a batch runs. Each
thread needs its own memory, so a high count on large scans can run out of memory.

## History and credits

- **ScanTailor** was written by Joseph Artsimovich.
- **ScanTailor Enhanced**, **Featured** and **Universal** were community variants that added features
  such as auto margins, page detection, deviation highlighting, picture shapes, marginal dewarping and
  auto-save.
- **ScanTailor Advanced** was started by [4lex4](https://github.com/4lex4/scantailor-advanced), who merged
  those variants and reworked many of their features.
- **ScanTailor-Advanced/scantailor-advanced** continued it after that repository went quiet: the Qt 6 port,
  translations (Polish by @ukolaj-s, French by @maltaisn, Korean by @brendan-t and @mirusu400), black
  filling, 1200 DPI, oblique correction, further binarization methods (Fox and Window come from
  zvezdochiot's work), and Linux packaging.
- **This branch** adds the macOS fixes listed [above](#this-branch-macos-on-apple-silicon).

## License

GNU GPLv3; see [LICENSE](LICENSE).
