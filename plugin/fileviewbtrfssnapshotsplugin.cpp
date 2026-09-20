/*
    SPDX-FileCopyrightText: 2026 The dolphin-btrfs contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fileviewbtrfssnapshotsplugin.h"

#include <KConfigGroup>
#include <KFileItem>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QAction>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMenu>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace {
constexpr auto snapshotDirectory = "/btrbk_snapshots";
constexpr auto configGroup = "BtrfsSnapshots";
constexpr auto configKey = "SnapshotDirectory";
constexpr auto recursiveVersionsKey = "RecursiveDirectoryVersions";

QString configuredSnapshotDirectory() {
  const KConfigGroup group(
      KSharedConfig::openConfig(QStringLiteral("dolphin-btrfsrc")),
      QString::fromLatin1(configGroup));
  return group.readEntry(QString::fromLatin1(configKey),
                         QString::fromLatin1(snapshotDirectory));
}

bool recursiveDirectoryVersions() {
  const KConfigGroup group(
      KSharedConfig::openConfig(QStringLiteral("dolphin-btrfsrc")),
      QString::fromLatin1(configGroup));
  return group.readEntry(QString::fromLatin1(recursiveVersionsKey), false);
}

struct Snapshot {
  QString name;
  QString timestamp;
  QString rootPath;
  QString mountPath;
  QString sortKey;
};

struct FileVersion {
  QByteArray fingerprint;
  qint64 size;
  QDateTime modified;
  bool directory;
};

bool sameVersion(const FileVersion &left, const FileVersion &right) {
  if (left.directory || right.directory) {
    return left.directory && right.directory &&
           left.fingerprint == right.fingerprint;
  }
  return left.size == right.size && left.modified == right.modified;
}

FileVersion versionFor(const QFileInfo &info, bool recursive) {
  if (!info.isDir()) {
    return {{}, info.size(), info.lastModified(), false};
  }

  QStringList metadata;
  const QDir root(info.absoluteFilePath());
  metadata.append(QStringLiteral(".\0dir\0%1")
                      .arg(info.lastModified().toMSecsSinceEpoch()));

  const auto addChild = [&metadata, &root](const QFileInfo &child) {
    const QString relativePath =
        root.relativeFilePath(child.absoluteFilePath());
    const QString type = child.isDir()       ? QStringLiteral("dir")
                         : child.isSymLink() ? QStringLiteral("link")
                                             : QStringLiteral("file");
    metadata.append(QStringLiteral("%1\0%2\0%3\0%4")
                        .arg(relativePath)
                        .arg(type)
                        .arg(child.size())
                        .arg(child.lastModified().toMSecsSinceEpoch()));
  };

  if (recursive) {
    QDirIterator iterator(info.absoluteFilePath(),
                          QDir::AllEntries | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
      iterator.next();
      addChild(iterator.fileInfo());
    }
  } else {
    for (const QFileInfo &child : root.entryInfoList(
             QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name)) {
      addChild(child);
    }
  }

  std::sort(metadata.begin(), metadata.end());
  QCryptographicHash hash(QCryptographicHash::Sha256);
  for (const QString &entry : metadata) {
    hash.addData(entry.toUtf8());
    hash.addData("\n");
  }
  return {hash.result(), 0, {}, true};
}

QList<Snapshot> btrbkSnapshots(const QString &snapshotDirectoryPath) {
  const QDir directory(snapshotDirectoryPath);
  const QRegularExpression pattern(
      QStringLiteral("^(home|ROOT)\\.(\\d{8}T\\d{4})$"));
  QList<Snapshot> snapshots;

  const QStringList entries =
      directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QString &entry : entries) {
    const QRegularExpressionMatch match = pattern.match(entry);
    if (match.hasMatch()) {
      const bool homeSnapshot = entry.startsWith(QStringLiteral("home."));
      snapshots.append(
          {entry, match.captured(2), directory.filePath(entry),
           homeSnapshot ? QStringLiteral("/home") : QStringLiteral("/"),
           match.captured(2)});
    }
  }

  std::sort(snapshots.begin(), snapshots.end(),
            [](const Snapshot &left, const Snapshot &right) {
              return left.timestamp > right.timestamp;
            });
  return snapshots;
}

QString snapperMountForPath(const QString &path) {
  QFileInfo pathInfo(path);
  QString candidate =
      pathInfo.isDir() ? pathInfo.absoluteFilePath() : pathInfo.absolutePath();

  while (true) {
    if (QFileInfo(QDir(candidate).filePath(QStringLiteral(".snapshots")))
            .isDir()) {
      return candidate;
    }

    const QString parent = QFileInfo(candidate).absolutePath();
    if (parent == candidate) {
      return {};
    }
    candidate = parent;
  }
}

QList<Snapshot> snapperSnapshots(const QString &path) {
  const QString mountPath = snapperMountForPath(path);
  if (mountPath.isEmpty()) {
    return {};
  }

  const QDir snapshotDirectory(
      QDir(mountPath).filePath(QStringLiteral(".snapshots")));
  const QRegularExpression idPattern(QStringLiteral("^\\d+$"));
  QList<Snapshot> snapshots;
  const QStringList entries = snapshotDirectory.entryList(
      QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QString &entry : entries) {
    if (!idPattern.match(entry).hasMatch()) {
      continue;
    }

    const QString rootPath =
        snapshotDirectory.filePath(entry + QStringLiteral("/snapshot"));
    if (!QFileInfo(rootPath).isDir()) {
      continue;
    }

    const QString identifier = QStringLiteral("snapper-") + entry;
    snapshots.append(
        {identifier, QStringLiteral("Snapshot ") + entry, rootPath, mountPath,
         QStringLiteral("snapper-") +
             QString(qMax(0, 20 - entry.size()), QLatin1Char('0')) + entry});
  }
  return snapshots;
}

QList<Snapshot> availableSnapshots(const QString &path,
                                   const QString &snapshotDirectoryPath) {
  QList<Snapshot> snapshots = btrbkSnapshots(snapshotDirectoryPath);
  snapshots.append(snapperSnapshots(path));
  std::sort(snapshots.begin(), snapshots.end(),
            [](const Snapshot &left, const Snapshot &right) {
              return left.sortKey > right.sortKey;
            });
  return snapshots;
}

QString snapshotPath(const QString &path, const Snapshot &snapshot) {
  QString relativePath = snapshot.mountPath == QStringLiteral("/")
                             ? path
                             : path.mid(snapshot.mountPath.size());
  if (relativePath.startsWith(QLatin1Char('/'))) {
    relativePath.remove(0, 1);
  }
  return QDir(snapshot.rootPath).filePath(relativePath);
}

QString displayTimestamp(const QString &timestamp) {
  if (timestamp.size() == 13 && timestamp.at(8) == QLatin1Char('T')) {
    return timestamp.left(4) + QLatin1Char('-') + timestamp.mid(4, 2) +
           QLatin1Char('-') + timestamp.mid(6, 2) + QLatin1Char(' ') +
           timestamp.mid(9, 2) + QLatin1Char(':') + timestamp.mid(11, 2);
  }
  return timestamp;
}
} // namespace

K_PLUGIN_CLASS_WITH_JSON(FileViewBtrfsSnapshotsPlugin,
                         "fileviewbtrfssnapshotsplugin.json")

FileViewBtrfsSnapshotsPlugin::FileViewBtrfsSnapshotsPlugin(
    QObject *parent, const QList<QVariant> &args)
    : KVersionControlPlugin(parent) {
  Q_UNUSED(args)

  m_snapshotMenu = new QMenu(qobject_cast<QWidget *>(parent));
  m_snapshotMenu->setTitle(i18nc("@title:menu", "Btrfs Snapshots"));
  m_snapshotMenu->setIcon(QIcon::fromTheme(QStringLiteral("drive-harddisk")));
}

FileViewBtrfsSnapshotsPlugin::~FileViewBtrfsSnapshotsPlugin() {
  if (m_snapshotMenu && !m_snapshotMenu->parent()) {
    delete m_snapshotMenu;
  }
}

QString FileViewBtrfsSnapshotsPlugin::fileName() const {
  // This plugin is not tied to a repository marker. Snapshot discovery is
  // global.
  return QStringLiteral(".btrfs-snapshots");
}

QString FileViewBtrfsSnapshotsPlugin::localRepositoryRoot(
    const QString &directory) const {
  Q_UNUSED(directory)
  return {};
}

bool FileViewBtrfsSnapshotsPlugin::beginRetrieval(const QString &directory) {
  Q_UNUSED(directory)
  return true;
}

void FileViewBtrfsSnapshotsPlugin::endRetrieval() {}

KVersionControlPlugin::ItemVersion
FileViewBtrfsSnapshotsPlugin::itemVersion(const KFileItem &item) const {
  Q_UNUSED(item)
  return UnversionedVersion;
}

QList<QAction *> FileViewBtrfsSnapshotsPlugin::versionControlActions(
    const KFileItemList &items) const {
  return snapshotActions(items);
}

QList<QAction *> FileViewBtrfsSnapshotsPlugin::outOfVersionControlActions(
    const KFileItemList &items) const {
  return snapshotActions(items);
}

QString
FileViewBtrfsSnapshotsPlugin::restorePath(const QString &path,
                                          const QString &snapshotName) const {
  const QFileInfo liveInfo(path);
  const QString basePath = liveInfo.absoluteDir().filePath(
      liveInfo.fileName() + QLatin1Char('.') + snapshotName);

  QString destinationPath = basePath;
  for (int suffix = 1; QFileInfo::exists(destinationPath); ++suffix) {
    destinationPath = basePath + QLatin1Char('.') + QString::number(suffix);
  }
  return destinationPath;
}

void FileViewBtrfsSnapshotsPlugin::restoreSnapshot(
    const QString &sourcePath, const QString &destinationPath) {
  auto *process = new QProcess(this);
  connect(process, &QProcess::errorOccurred, this,
          [this, process, destinationPath](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
              Q_EMIT errorMessage(i18nc("@info:status",
                                        "Could not restore the snapshot to %1.",
                                        destinationPath));
              process->deleteLater();
            }
          });
  connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this,
          [this, process, destinationPath](int exitCode,
                                           QProcess::ExitStatus exitStatus) {
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
              Q_EMIT operationCompletedMessage(i18nc(
                  "@info:status", "Restored snapshot to %1.", destinationPath));
            } else {
              Q_EMIT errorMessage(i18nc("@info:status",
                                        "Could not restore the snapshot to %1.",
                                        destinationPath));
            }
            process->deleteLater();
          });

  process->start(QStringLiteral("cp"),
                 {QStringLiteral("--archive"),
                  QStringLiteral("--reflink=always"),
                  QStringLiteral("--no-clobber"), sourcePath, destinationPath});
}

QList<QAction *> FileViewBtrfsSnapshotsPlugin::snapshotActions(
    const KFileItemList &items) const {
  // A single target keeps every menu entry unambiguous. It also avoids
  // accidentally opening a snapshot belonging to a different subvolume.
  if (items.size() != 1 || !items.first().isLocalFile()) {
    return {};
  }

  const QString path = items.first().localPath();
  const QFileInfo liveInfo(path);
  const bool hasLiveVersion = liveInfo.exists();
  const bool recursive = recursiveDirectoryVersions();
  const FileVersion liveVersion = versionFor(liveInfo, recursive);
  QList<FileVersion> seenVersions;
  const QList<QMenu *> oldMenus = m_snapshotMenu->findChildren<QMenu *>(
      QString(), Qt::FindDirectChildrenOnly);
  m_snapshotMenu->clear();
  for (QMenu *menu : oldMenus) {
    delete menu;
  }

  for (const Snapshot &snapshot :
       availableSnapshots(path, configuredSnapshotDirectory())) {
    const QString historicalPath = ::snapshotPath(path, snapshot);
    const QFileInfo snapshotInfo(historicalPath);
    if (!snapshotInfo.exists()) {
      continue;
    }

    const FileVersion snapshotVersion = versionFor(snapshotInfo, recursive);
    if (hasLiveVersion && sameVersion(snapshotVersion, liveVersion)) {
      continue;
    }

    if (std::any_of(seenVersions.cbegin(), seenVersions.cend(),
                    [&snapshotVersion](const FileVersion &seenVersion) {
                      return sameVersion(seenVersion, snapshotVersion);
                    })) {
      continue;
    }
    seenVersions.append(snapshotVersion);

    auto *snapshotMenu =
        new QMenu(i18nc("@title:menu", "%1 (%2)",
                        displayTimestamp(snapshot.timestamp), snapshot.name),
                  m_snapshotMenu);
    snapshotMenu->setIcon(
        QIcon::fromTheme(QStringLiteral("document-open-recent")));

    auto *openAction = snapshotMenu->addAction(
        QIcon::fromTheme(QStringLiteral("document-open")),
        i18nc("@action:inmenu", "Open"));
    connect(openAction, &QAction::triggered, this, [historicalPath]() {
      auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(historicalPath));
      job->start();
    });

    const QString destinationPath = restorePath(path, snapshot.name);
    auto *restoreAction = snapshotMenu->addAction(
        QIcon::fromTheme(QStringLiteral("document-save-as")),
        i18nc("@action:inmenu", "Restore"));
    auto *plugin = const_cast<FileViewBtrfsSnapshotsPlugin *>(this);
    connect(restoreAction, &QAction::triggered, plugin,
            [plugin, historicalPath, destinationPath]() {
              plugin->restoreSnapshot(historicalPath, destinationPath);
            });

    snapshotMenu->setDefaultAction(openAction);
    m_snapshotMenu->addMenu(snapshotMenu);
  }

  if (m_snapshotMenu->actions().isEmpty()) {
    return {};
  }

  return {m_snapshotMenu->menuAction()};
}

#include "fileviewbtrfssnapshotsplugin.moc"
