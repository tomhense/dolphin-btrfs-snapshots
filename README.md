# Dolphin Btrfs snapshots plugin

This plugin adds a **Btrfs Snapshots** context-menu section to Dolphin. It
lists existing versions of a selected local file or directory from btrbk
snapshots and opens the selected version.

The initial layout is intentionally limited to:

```text
/btrbk_snapshots/home.YYYYMMDDTHHMM
/btrbk_snapshots/ROOT.YYYYMMDDTHHMM
```

`/home/...` is looked up in the `home.*` snapshots; all other absolute paths
are looked up in the `ROOT.*` snapshots. Only snapshots containing the
selected path are shown.

Build and install with the normal KDE CMake workflow:

```sh
cmake -S . -B build
cmake --build build
cmake --install build
```

The KDE Extra CMake Modules, Qt6, KF6 KIO/I18n, and DolphinVcs development
packages are required.
