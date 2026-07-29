#include "PathUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace {

QString cleanPath(const QString& path) {
    return QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
}

bool hasWindowsDrivePrefix(const QString& path) {
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    return normalized.size() >= 2 && normalized.at(1) == QLatin1Char(':') && normalized.at(0).isLetter();
}

QString windowsDrivePrefix(const QString& path) {
    if (!hasWindowsDrivePrefix(path)) {
        return QString();
    }
    return QDir::fromNativeSeparators(path.trimmed()).left(2).toUpper();
}

bool isUncPath(const QString& path) {
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    return normalized.startsWith(QString::fromLatin1("//"));
}

bool isPortableRelativePath(const QString& path) {
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    if (normalized.isEmpty() || QDir::isAbsolutePath(normalized) || hasWindowsDrivePrefix(normalized)) {
        return false;
    }
    return normalized.startsWith(QString::fromLatin1("./")) || normalized.startsWith(QString::fromLatin1("../")) ||
           normalized == QString::fromLatin1(".") || normalized == QString::fromLatin1("..") ||
           normalized.contains(QLatin1Char('/'));
}

bool isSameLocalDrive(const QString& path, const QString& baseDirectory) {
    if (isUncPath(path) || isUncPath(baseDirectory)) {
        return false;
    }
    const QString pathDrive = windowsDrivePrefix(path);
    const QString baseDrive = windowsDrivePrefix(baseDirectory);
    return !pathDrive.isEmpty() && pathDrive == baseDrive;
}

QString withExplicitCurrentDirectoryPrefix(const QString& relativePath) {
    QString normalized = cleanPath(relativePath);
    if (normalized == QString::fromLatin1(".")) {
        return normalized;
    }
    if (!normalized.contains(QLatin1Char('/')) && !normalized.startsWith(QString::fromLatin1("../")) &&
        !normalized.startsWith(QString::fromLatin1("./"))) {
        normalized.prepend(QString::fromLatin1("./"));
    }
    return normalized;
}

} // namespace

namespace PathUtils {

QString launcherBaseDirectory() {
    return QDir::cleanPath(QCoreApplication::applicationDirPath());
}

QString toPortablePath(const QString& path, const QString& baseDirectory) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || baseDirectory.trimmed().isEmpty()) {
        return trimmed;
    }

    const QString normalizedPath = cleanPath(trimmed);
    const QString normalizedBase = cleanPath(baseDirectory);
    if (!QDir::isAbsolutePath(normalizedPath) || !isSameLocalDrive(normalizedPath, normalizedBase)) {
        return trimmed;
    }

    return withExplicitCurrentDirectoryPrefix(QDir(normalizedBase).relativeFilePath(normalizedPath));
}

QString toPortablePath(const QString& path) {
    return toPortablePath(path, launcherBaseDirectory());
}

QString toAbsolutePath(const QString& path, const QString& baseDirectory) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || baseDirectory.trimmed().isEmpty()) {
        return trimmed;
    }

    if (!isPortableRelativePath(trimmed)) {
        return trimmed;
    }

    return QDir::cleanPath(QDir(cleanPath(baseDirectory)).absoluteFilePath(QDir::fromNativeSeparators(trimmed)));
}

QString toAbsolutePath(const QString& path) {
    return toAbsolutePath(path, launcherBaseDirectory());
}

LaunchAction toPortableAction(const LaunchAction& action, const QString& baseDirectory) {
    LaunchAction converted = action;
    if (converted.type != LaunchActionType::Url) {
        converted.target = toPortablePath(converted.target, baseDirectory);
        converted.workingDirectory = toPortablePath(converted.workingDirectory, baseDirectory);
    }
    return converted;
}

LaunchAction toPortableAction(const LaunchAction& action) {
    return toPortableAction(action, launcherBaseDirectory());
}

LaunchAction toAbsoluteAction(const LaunchAction& action, const QString& baseDirectory) {
    LaunchAction converted = action;
    if (converted.type != LaunchActionType::Url) {
        converted.target = toAbsolutePath(converted.target, baseDirectory);
        converted.workingDirectory = toAbsolutePath(converted.workingDirectory, baseDirectory);
    }
    return converted;
}

LaunchAction toAbsoluteAction(const LaunchAction& action) {
    return toAbsoluteAction(action, launcherBaseDirectory());
}

} // namespace PathUtils