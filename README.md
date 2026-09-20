# Dolphin Btrfs snapshots plugin

This plugin adds a **Btrfs Snapshots** context-menu section to Dolphin. It
lists existing versions of a selected local file or directory from btrbk
snapshots and opens the selected version.

The initial layout is intentionally limited to:

```text
/btrbk_snapshots/home.YYYYMMDDTHHMM
/btrbk_snapshots/ROOT.YYYYMMDDTHHMM
```

The snapshot directory defaults to `/btrbk_snapshots` and can be changed in
the KDE config file `~/.config/dolphin-btrfsrc`:

```ini
[BtrfsSnapshots]
SnapshotDirectory=/btrbk_snapshots
```

The configured path must be absolute. `XDG_CONFIG_HOME` is honored when it is
set.

In addition to the configured btrbk layout, the plugin automatically detects
mounted Snapper snapshots below the nearest live subvolume mount:

```text
<mount>/.snapshots/<id>/snapshot
```

For example, `/home/.snapshots/42/snapshot/tom/file.txt` is considered a
version of `/home/tom/file.txt`. Snapper snapshot IDs are shown in the menu.

`/home/...` is looked up in the `home.*` snapshots; all other absolute paths
are looked up in the `ROOT.*` snapshots. Only snapshots containing the
selected path are shown. Snapshot entries with the same size and modification
time are collapsed to the newest entry, and entries matching the live item are
omitted. For directories, all matching snapshots are shown because directory
metadata does not reliably describe changes to their contents. This avoids
scanning directory trees while building the context menu.

Each snapshot has an **Open** action and a **Restore** action. Restore creates
a new sibling copy, for example `file.home.20260919T1901`, using a required
reflink; it never overwrites the original or an existing restore copy.

Build and install with the normal KDE CMake workflow:

```sh
cmake -S . -B build
cmake --build build
cmake --install build
```

The KDE Extra CMake Modules, Qt6, KF6 KIO/I18n, and DolphinVcs development
packages are required.

## Permanent installation on Arch Linux

Install the build dependencies:

```sh
sudo pacman -S --needed base-devel cmake ninja extra-cmake-modules \
    qt6-base kio ki18n dolphin
```

For a system-wide installation that Dolphin finds automatically:

```sh
cmake --fresh -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

The plugin is installed below Qt’s plugin directory at
`dolphin/vcs/fileviewbtrfssnapshotsplugin.so`. Restart Dolphin after
installation. The installation can be verified with:

```sh
find /usr -path '*/dolphin/vcs/fileviewbtrfssnapshotsplugin.so' -print
```

Alternatively, install for the current user:

```sh
cmake --fresh -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

For a user-local installation, launch Dolphin with the local Qt plugin path:

```sh
QT_PLUGIN_PATH="$HOME/.local/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
    dolphin --new-instance
```
