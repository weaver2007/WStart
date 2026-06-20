#pragma once

#include <QApplication>
#include <QGuiApplication>
#include <QRect>
#include <QScreen>
#include <QString>
#include <QUuid>
#include <QWidget>

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
#include <QDesktopWidget>
#endif

namespace QtCompat {

inline QString uuidWithoutBraces() {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
#else
    QString value = QUuid::createUuid().toString();
    if (value.startsWith('{') && value.endsWith('}')) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
#endif
}

inline int boundedInt(int value, int minimum, int maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

inline int scaleInt(int value) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    static int scalePercent = []() -> int {
        QDesktopWidget* desktop = QApplication::desktop();
        const int dpi = desktop ? desktop->logicalDpiX() : 96;
        if (dpi <= 96) {
            return 100;
        }
        return boundedInt((dpi * 100 + 48) / 96, 100, 220);
    }();
    return (value * scalePercent + 50) / 100;
#else
    return value;
#endif
}

inline QString sanitizeFileName(QString value) {
    static const QChar replacement('_');
    static const QString illegal = QString::fromLatin1("\\/:*?\"<>|");
    for (int i = 0; i < value.size(); ++i) {
        if (illegal.contains(value.at(i))) {
            value[i] = replacement;
        }
    }
    return value;
}

inline QScreen* screenAtPoint(const QPoint& point) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    return QGuiApplication::screenAt(point);
#else
    QDesktopWidget* desktop = QApplication::desktop();
    if (!desktop) {
        return QGuiApplication::primaryScreen();
    }
    const int screenIndex = desktop->screenNumber(point);
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screenIndex >= 0 && screenIndex < screens.size()) {
        return screens.at(screenIndex);
    }
    return QGuiApplication::primaryScreen();
#endif
}

inline QScreen* screenForWidget(const QWidget* widget) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    if (widget) {
        if (QScreen* screen = widget->screen()) {
            return screen;
        }
    }
#endif
    if (widget) {
        if (QScreen* screen = screenAtPoint(widget->frameGeometry().center())) {
            return screen;
        }
    }
    return QGuiApplication::primaryScreen();
}

} // namespace QtCompat
