/*
    SPDX-FileCopyrightText: 2026 The dolphin-btrfs contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fileviewbtrfssnapshotsplugin.h"

#include <KFileItem>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace {
constexpr auto snapshotDirectory = "/btrbk_snapshots";

struct Snapshot {
  QString name;
  QString timestamp;
};

QList<Snapshot> availableSnapshots() {
  const QDir directory(QString::fromLatin1(snapshotDirectory));
  const QRegularExpression pattern(
      QStringLiteral("^(home|ROOT)\\.(\\d{8}T\\d{4})$"));
  QList<Snapshot> snapshots;

  const QStringList entries =
      directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QString &entry : entries) {
    const QRegularExpressionMatch match = pattern.match(entry);
    if (match.hasMatch()) {
      snapshots.append({entry, match.captured(2)});
    }
  }

  std::sort(snapshots.begin(), snapshots.end(),
            [](const Snapshot &left, const Snapshot &right) {
              return left.timestamp > right.timestamp;
            });
  return snapshots;
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
FileViewBtrfsSnapshotsPlugin::snapshotPath(const QString &path,
                                           const QString &snapshotName) const {
  // btrbk stores the home subvolume without the /home component. The ROOT
  // subvolume contains the complete filesystem hierarchy.
  const bool isHomeSubvolume = path == QStringLiteral("/home") ||
                               path.startsWith(QStringLiteral("/home/"));
  const QString relativePath =
      isHomeSubvolume ? path.mid(QStringLiteral("/home").size()) : path;
  return QDir(QString::fromLatin1(snapshotDirectory))
      .filePath(snapshotName + relativePath);
}

QList<QAction *> FileViewBtrfsSnapshotsPlugin::snapshotActions(
    const KFileItemList &items) const {
  // A single target keeps every menu entry unambiguous. It also avoids
  // accidentally opening a snapshot belonging to a different subvolume.
  if (items.size() != 1 || !items.first().isLocalFile()) {
    return {};
  }

  const QString path = items.first().localPath();
  const QList<QAction *> oldActions = m_snapshotMenu->actions();
  m_snapshotMenu->clear();
  for (QAction *action : oldActions) {
    delete action;
  }

  for (const Snapshot &snapshot : availableSnapshots()) {
    const QString historicalPath = snapshotPath(path, snapshot.name);
    if (!QFileInfo::exists(historicalPath)) {
      continue;
    }

    auto *action = new QAction(m_snapshotMenu);
    action->setText(i18nc("@action:inmenu", "%1 (%2)",
                          displayTimestamp(snapshot.timestamp), snapshot.name));
    action->setIcon(QIcon::fromTheme(QStringLiteral("document-open-recent")));
    action->setToolTip(historicalPath);
    connect(action, &QAction::triggered, this, [historicalPath]() {
      auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(historicalPath));
      job->start();
    });
    m_snapshotMenu->addAction(action);
  }

  if (m_snapshotMenu->actions().isEmpty()) {
    return {};
  }

  return {m_snapshotMenu->menuAction()};
}

#include "fileviewbtrfssnapshotsplugin.moc"
