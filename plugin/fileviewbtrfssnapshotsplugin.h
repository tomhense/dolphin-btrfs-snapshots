/*
    SPDX-FileCopyrightText: 2026 The dolphin-btrfs contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FILEVIEWBTRFSSNAPSHOTSPLUGIN_H
#define FILEVIEWBTRFSSNAPSHOTSPLUGIN_H

#include <Dolphin/KVersionControlPlugin>

#include <QList>
#include <QString>

class QMenu;

class FileViewBtrfsSnapshotsPlugin : public KVersionControlPlugin {
  Q_OBJECT

public:
  explicit FileViewBtrfsSnapshotsPlugin(QObject *parent,
                                        const QList<QVariant> &args);
  ~FileViewBtrfsSnapshotsPlugin() override;

  QString fileName() const override;
  QString localRepositoryRoot(const QString &directory) const override;
  bool beginRetrieval(const QString &directory) override;
  void endRetrieval() override;
  ItemVersion itemVersion(const KFileItem &item) const override;
  QList<QAction *>
  versionControlActions(const KFileItemList &items) const override;
  QList<QAction *>
  outOfVersionControlActions(const KFileItemList &items) const override;

private:
  QList<QAction *> snapshotActions(const KFileItemList &items) const;
  QString snapshotPath(const QString &path, const QString &snapshotName) const;
  QString restorePath(const QString &path, const QString &snapshotName) const;
  void restoreSnapshot(const QString &sourcePath,
                       const QString &destinationPath);

  mutable QMenu *m_snapshotMenu = nullptr;
};

#endif
