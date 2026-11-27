#pragma once

#include <QCoreApplication>

inline QString appPath(const QString &relativePath)
{
#ifdef APPIMAGE_BUILD
    return QCoreApplication::applicationDirPath() + QStringLiteral("/../") + relativePath;
#else
    return QStringLiteral("/usr/") + relativePath;
#endif
}
