#pragma once

#include "HotkeyTypes.h"

#include <QStringList>

class HotkeyConflictDetector {
public:
    enum class Availability { Invalid, Available, RegisteredByOtherApp, SystemReserved };

    struct Result {
        Availability availability = Availability::Invalid;
        QStringList notes;
    };

    static Result check(const HotkeyCombination& hotkey, const QString& language);
    static bool isKnownSystemHotkey(const HotkeyCombination& hotkey);

private:
    static QStringList systemHotkeyNames();
};
