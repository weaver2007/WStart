#pragma once

#include <QString>

namespace BuiltInActions {

inline QString moveActiveWindowToNextMonitorTarget() {
    return QString::fromLatin1("wstart:move-active-window-to-next-monitor");
}

inline bool isMoveActiveWindowToNextMonitor(const QString& target) {
    return target.trimmed().compare(moveActiveWindowToNextMonitorTarget(), Qt::CaseInsensitive) == 0;
}

inline bool isBuiltInTarget(const QString& target) {
    return isMoveActiveWindowToNextMonitor(target);
}

} // namespace BuiltInActions
