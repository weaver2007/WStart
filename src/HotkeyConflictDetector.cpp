#include "HotkeyConflictDetector.h"

#include "UiText.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int ProbeHotkeyId = 0x48535431;

#ifdef Q_OS_WIN
UINT nativeModifiers(HotkeyModifiers modifiers) {
    UINT result = 0;
#if defined(MOD_NOREPEAT)
    result |= MOD_NOREPEAT;
#endif
    if (modifiers.testFlag(ModifierCtrl)) {
        result |= MOD_CONTROL;
    }
    if (modifiers.testFlag(ModifierAlt)) {
        result |= MOD_ALT;
    }
    if (modifiers.testFlag(ModifierShift)) {
        result |= MOD_SHIFT;
    }
    if (modifiers.testFlag(ModifierWin)) {
        result |= MOD_WIN;
    }
    return result;
}
#endif

} // namespace

HotkeyConflictDetector::Result HotkeyConflictDetector::check(const HotkeyCombination& hotkey, const QString& language) {
    Result result;
    const QString normalizedLanguage = UiText::normalizeLanguage(language);
    if (!hotkey.isValid()) {
        result.availability = Availability::Invalid;
        result.notes << UiText::text(normalizedLanguage, UiText::Key::HotkeyCheckInvalid);
        return result;
    }

    if (isKnownSystemHotkey(hotkey)) {
        result.availability = Availability::SystemReserved;
        result.notes << UiText::text(normalizedLanguage, UiText::Key::HotkeyCheckSystemReserved);
        result.notes << UiText::text(normalizedLanguage, UiText::Key::HotkeyCheckBestEffort);
        return result;
    }

#ifdef Q_OS_WIN
    const HWND windowHandle = nullptr;
    const BOOL registered =
        RegisterHotKey(windowHandle, ProbeHotkeyId, nativeModifiers(hotkey.modifiers), static_cast<UINT>(hotkey.key));
    if (!registered) {
        result.availability = Availability::RegisteredByOtherApp;
        result.notes
            << UiText::text(normalizedLanguage, UiText::Key::HotkeyCheckRegisteredByOtherApp).arg(GetLastError());
        result.notes << UiText::text(normalizedLanguage, UiText::Key::HotkeyCheckCannotForceDisable);
        return result;
    }
    UnregisterHotKey(windowHandle, ProbeHotkeyId);
    result.availability = Availability::Available;
    result.notes << UiText::text(normalizedLanguage, UiText::Key::HotkeyCheckAvailable);
    return result;
#else
    result.availability = Availability::Invalid;
    result.notes << UiText::text(normalizedLanguage, UiText::Key::HotkeyCheckWindowsOnly);
    return result;
#endif
}

bool HotkeyConflictDetector::isKnownSystemHotkey(const HotkeyCombination& hotkey) {
    return systemHotkeyNames().contains(hotkey.displayText(), Qt::CaseInsensitive);
}

QStringList HotkeyConflictDetector::systemHotkeyNames() {
    QStringList names;
    names << "Win+S" << "Win+E" << "Win+R" << "Win+D" << "Win+Tab" << "Win+L" << "Win+V"
          << "Win+A" << "Win+I" << "Win+X" << "Win+P" << "Win+K" << "Win+H"
          << "Alt+Tab" << "Alt+F4" << "Ctrl+Esc" << "Ctrl+Shift+Esc";
    return names;
}
