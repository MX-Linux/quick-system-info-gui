#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

inline QString appPath(const QString &relativePath)
{
    const QDir prefix(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral(QSI_PREFIX_FROM_BIN)));
#ifndef APPIMAGE_BUILD
    // A developer running a build-tree executable can use the distro resources.
    if (!QFileInfo::exists(prefix.filePath(QStringLiteral("lib/quick-system-info-gui/qsig-lib"))))
        return QStringLiteral("/usr/") + relativePath;
#endif
    return QDir::cleanPath(prefix.filePath(relativePath));
}

inline QString reportToolPath(const QString &name)
{
#ifdef APPIMAGE_BUILD
    return appPath(QStringLiteral("bin/") + name);
#else
    return QStringLiteral("/usr/bin/") + name;
#endif
}
