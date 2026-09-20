# Dolphin Btrfs snapshots plugin

This plugin adds a **Btrfs Snapshots** context-menu section to Dolphin. It
lists existing versions of a selected local file or directory from btrbk
snapshots and opens the selected version.

The initial layout is intentionally limited to:

```text
/btrbk_snapshots/home.YYYYMMDDTHHMM
/btrbk_snapshots/ROOT.YYYYMMDDTHHMM
```

The snapshot directory defaults to `/btrbk_snapshots` and can be changed via
**Btrfs Snapshots → Configure Btrfs Snapshots…** in Dolphin. The configured
path must be absolute.

`/home/...` is looked up in the `home.*` snapshots; all other absolute paths
are looked up in the `ROOT.*` snapshots. Only snapshots containing the
selected path are shown. Snapshot entries with the same size and modification
time are collapsed to the newest entry, and entries matching the live item are
omitted. This automatically removes redundant snapshots regardless of how long
the item has remained unchanged.

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
