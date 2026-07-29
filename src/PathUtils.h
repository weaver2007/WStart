#pragma once

#include "HotkeyTypes.h"

#include <QString>

namespace PathUtils {

QString launcherBaseDirectory();
QString toPortablePath(const QString& path, const QString& baseDirectory);
QString toPortablePath(const QString& path);
QString toAbsolutePath(const QString& path, const QString& baseDirectory);
QString toAbsolutePath(const QString& path);
LaunchAction toPortableAction(const LaunchAction& action, const QString& baseDirectory);
LaunchAction toPortableAction(const LaunchAction& action);
LaunchAction toAbsoluteAction(const LaunchAction& action, const QString& baseDirectory);
LaunchAction toAbsoluteAction(const LaunchAction& action);

} // namespace PathUtils