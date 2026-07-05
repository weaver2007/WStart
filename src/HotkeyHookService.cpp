#include "HotkeyHookService.h"

#include "AppLogger.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QMetaType>

#ifdef Q_OS_WIN
HotkeyHookService* HotkeyHookService::s_instance = nullptr;

namespace {
const ULONG_PTR kCancelStartMenuExtraInfo = static_cast<ULONG_PTR>(0x57535457u);

bool isCancelStartMenuInput(const KBDLLHOOKSTRUCT* event) {
    return event && (event->flags & LLKHF_INJECTED) && event->dwExtraInfo == kCancelStartMenuExtraInfo;
}

bool isWinKey(int virtualKey) {
    return virtualKey == VK_LWIN || virtualKey == VK_RWIN;
}

QString boolText(bool value) {
    return value ? QString::fromLatin1("1") : QString::fromLatin1("0");
}

QString keyName(int virtualKey) {
    switch (virtualKey) {
    case VK_LWIN:
        return QString::fromLatin1("VK_LWIN");
    case VK_RWIN:
        return QString::fromLatin1("VK_RWIN");
    case VK_F24:
        return QString::fromLatin1("VK_F24");
    default:
        return QString::fromLatin1("VK_0x%1").arg(virtualKey, 0, 16).toUpper();
    }
}

QString eventName(bool isKeyDown, bool isKeyUp) {
    if (isKeyDown) {
        return QString::fromLatin1("down");
    }
    if (isKeyUp) {
        return QString::fromLatin1("up");
    }
    return QString::fromLatin1("other");
}

QString flagsText(DWORD flags) {
    return QString::fromLatin1("0x%1").arg(static_cast<qulonglong>(flags), 0, 16).toUpper();
}

QString hookStateText(bool leftWinDown, bool rightWinDown, HotkeyModifiers pressedModifiers) {
    return QString::fromLatin1("leftWin=%1 rightWin=%2 pressedMods=%3")
        .arg(boolText(leftWinDown), boolText(rightWinDown))
        .arg(static_cast<int>(pressedModifiers));
}

void logHotkeyState(const QString& message) {
    AppLogger::writeLine(QString::fromLatin1("HOTKEY"), message);
}

bool isPhysicalKeyDown(int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

void sendCancelStartMenuInput() {
    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_F24;
    inputs[0].ki.dwExtraInfo = kCancelStartMenuExtraInfo;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_F24;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[1].ki.dwExtraInfo = kCancelStartMenuExtraInfo;

    SendInput(2, inputs, sizeof(INPUT));
}

void sendWinKeyUp(int virtualKey) {
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(virtualKey);
    input.ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
    SendInput(1, &input, sizeof(INPUT));
}

void resetSystemWinKeyState(const QString& reason) {
    const bool leftDown = isPhysicalKeyDown(VK_LWIN);
    const bool rightDown = isPhysicalKeyDown(VK_RWIN);
    logHotkeyState(QString::fromLatin1("%1 reset-system-win-state beforeLeft=%2 beforeRight=%3")
                       .arg(reason, boolText(leftDown), boolText(rightDown)));

    if (leftDown) {
        sendWinKeyUp(VK_LWIN);
    }
    if (rightDown) {
        sendWinKeyUp(VK_RWIN);
    }
}
} // namespace
#endif

HotkeyHookService::HotkeyHookService(QObject* parent) : QObject(parent) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    qRegisterMetaType<HotkeyRule>("HotkeyRule");
    connect(this, SIGNAL(queuedHotkeyTriggered(HotkeyRule)), this, SIGNAL(hotkeyTriggered(HotkeyRule)),
            Qt::QueuedConnection);
#else
    connect(this, &HotkeyHookService::queuedHotkeyTriggered, this, &HotkeyHookService::hotkeyTriggered,
            Qt::QueuedConnection);
#endif
}

HotkeyHookService::~HotkeyHookService() {
    stop();
}

bool HotkeyHookService::start(QString* error) {
#ifdef Q_OS_WIN
    if (m_hook) {
        return true;
    }
    resetSystemWinKeyState(QString::fromLatin1("pre-hook-start"));
    s_instance = this;
    resetKeyboardState(QString::fromLatin1("hook-start"));
    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, &HotkeyHookService::keyboardProc, GetModuleHandleW(nullptr), 0);

    if (!m_hook) {
        const DWORD lastError = GetLastError();
        logHotkeyState(QString::fromLatin1("hook-start-failed error=%1").arg(lastError));
        if (error) {
            *error = QString("SetWindowsHookEx failed with error %1.").arg(lastError);
        }
        s_instance = nullptr;
        return false;
    }
    logHotkeyState(QString::fromLatin1("hook-started reset-win-state %1")
                       .arg(hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_pressedModifiers)));
    return true;
#else
    Q_UNUSED(error)
    // Non-Windows platforms keep the UI and rule management available first.
    // Platform-specific global hotkey backends can be added without changing callers.
    return true;
#endif
}

void HotkeyHookService::stop() {
#ifdef Q_OS_WIN
    const bool hadHook = m_hook != nullptr;
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    if (hadHook) {
        resetSystemWinKeyState(QString::fromLatin1("post-hook-stop"));
    }
    if (s_instance == this) {
        resetKeyboardState(QString::fromLatin1("hook-stopped"));
        s_instance = nullptr;
    }
#endif
}

bool HotkeyHookService::isRunning() const {
#ifdef Q_OS_WIN
    return m_hook != nullptr;
#else
    return false;
#endif
}

void HotkeyHookService::setPaused(bool paused) {
    m_paused = paused;
#ifdef Q_OS_WIN
    logHotkeyState(QString::fromLatin1("set-paused paused=%1").arg(boolText(paused)));
#endif
#ifdef Q_OS_WIN
    resetKeyboardState(paused ? QString::fromLatin1("paused") : QString::fromLatin1("resumed"));
    if (paused) {
        resetSystemWinKeyState(QString::fromLatin1("paused"));
    }
#else
    if (paused) {
        m_activeTriggers.clear();
    }
#endif
}

bool HotkeyHookService::isPaused() const {
    return m_paused;
}

void HotkeyHookService::setRules(const QVector<HotkeyRule>& rules) {
    m_rules = rules;
}

#ifdef Q_OS_WIN
LRESULT CALLBACK HotkeyHookService::keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && s_instance) {
        const auto* event = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        const LRESULT result = s_instance->handleKeyboardEvent(wParam, event);
        if (result != 0) {
            return result;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT HotkeyHookService::handleKeyboardEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* event) {
    if (m_paused || !event) {
        return 0;
    }

    if (isCancelStartMenuInput(event)) {
        return 0;
    }

    const bool isKeyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool isKeyUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!isKeyDown && !isKeyUp) {
        return 0;
    }

    const int virtualKey = static_cast<int>(event->vkCode);
    const bool isModifier = isModifierKey(virtualKey);
    const bool leftWinBefore = m_leftWinPhysicallyDown;
    const bool rightWinBefore = m_rightWinPhysicallyDown;

    if (isWinKey(virtualKey)) {
        if (virtualKey == VK_LWIN) {
            m_leftWinPhysicallyDown = isKeyDown;
        } else {
            m_rightWinPhysicallyDown = isKeyDown;
        }
        logHotkeyState(QString::fromLatin1("win-event event=%1 key=%2 flags=%3 injected=%4 %5")
                           .arg(eventName(isKeyDown, isKeyUp), keyName(virtualKey), flagsText(event->flags),
                                boolText((event->flags & LLKHF_INJECTED) != 0),
                                hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_pressedModifiers)));
    } else {
        m_leftWinPhysicallyDown = isPhysicalKeyDown(VK_LWIN);
        m_rightWinPhysicallyDown = isPhysicalKeyDown(VK_RWIN);
        if (leftWinBefore != m_leftWinPhysicallyDown || rightWinBefore != m_rightWinPhysicallyDown) {
            logHotkeyState(QString::fromLatin1("async-win-state-cleared currentKey=%1 event=%2 beforeLeft=%3 beforeRight=%4 %5")
                               .arg(keyName(virtualKey), eventName(isKeyDown, isKeyUp), boolText(leftWinBefore),
                                    boolText(rightWinBefore),
                                    hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_pressedModifiers)));
        }
    }
    if (isKeyDown && isModifier) {
        updatePressedModifierState(virtualKey, true);
    }
    if (isKeyUp && isModifier) {
        updatePressedModifierState(virtualKey, false);
    }

    // Once a chord is claimed, swallow the key-up events that belong to it too.
    // Modifier releases must continue to the system; swallowing Win-up leaves
    // Windows thinking the Win key is still pressed for subsequent keystrokes.
    if (isTrackedSuppressedKey(virtualKey)) {
        logHotkeyState(QString::fromLatin1("swallow-suppressed event=%1 key=%2 %3")
                           .arg(eventName(isKeyDown, isKeyUp), keyName(virtualKey),
                                hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_pressedModifiers)));
        if (isKeyUp) {
            const QString triggerId = m_suppressedTriggerIds.take(virtualKey);
            if (!triggerId.isEmpty()) {
                m_activeTriggers.remove(triggerId);
                logHotkeyState(QString::fromLatin1("trigger-finished trigger=%1 key=%2").arg(triggerId, keyName(virtualKey)));
            }
            clearSuppressedKey(virtualKey);
        }
        return 1;
    }


    const HotkeyModifiers modifiers = currentModifiers(virtualKey, isKeyDown, isKeyUp);
    const HotkeyRule* rule = matchingRule(virtualKey, modifiers);
    if (!rule) {
        if (isKeyUp && isModifier) {
            clearSuppressedKey(virtualKey);
        }
        return 0;
    }

    const QString triggerId = rule->hotkey.stableId();
    if (isKeyUp) {
        m_activeTriggers.remove(triggerId);
        clearSuppressedKey(virtualKey);
        if (isModifier) {
            updatePressedModifierState(virtualKey, false);
        }
        logHotkeyState(QString::fromLatin1("swallow-rule-keyup trigger=%1 key=%2 %3")
                           .arg(triggerId, keyName(virtualKey),
                                hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_pressedModifiers)));
        return 1;
    }

    if (!m_activeTriggers.contains(triggerId)) {
        m_activeTriggers.insert(triggerId);
        logHotkeyState(QString::fromLatin1("trigger-start rule=%1 hotkey=%2 key=%3 modifiers=%4 %5")
                           .arg(rule->id, rule->hotkey.displayText(), keyName(virtualKey))
                           .arg(static_cast<int>(modifiers))
                           .arg(hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_pressedModifiers)));
        // The low-level hook sees each key as a separate event; remember the
        // non-modifier key so repeated key-down messages do not re-launch the action.
        trackSuppressedChord(rule->hotkey);
        m_suppressedTriggerIds.insert(rule->hotkey.key, triggerId);
        if (rule->hotkey.modifiers.testFlag(ModifierWin)) {
            // The real trigger key is swallowed, so the shell would otherwise
            // see a lone Win press. Send a no-op key while Win is still down so
            // Start-menu activation is cancelled without hiding the Win release.
            sendCancelStartMenuInput();
            logHotkeyState(QString::fromLatin1("cancel-start-menu-input trigger=%1 %2")
                               .arg(triggerId,
                                    hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_pressedModifiers)));
        }
        const HotkeyRule ruleCopy = *rule;
        emit queuedHotkeyTriggered(ruleCopy);
    } else {
        logHotkeyState(QString::fromLatin1("trigger-repeat-ignored trigger=%1 key=%2")
                           .arg(triggerId, keyName(virtualKey)));
    }
    if (isModifier) {
        updatePressedModifierState(virtualKey, true);
    }
    return 1;
}

HotkeyModifiers HotkeyHookService::currentModifiers(int eventKey, bool isKeyDown, bool isKeyUp) {
    HotkeyModifiers modifiers = ModifierNone;

    auto pressed = [eventKey, isKeyDown, isKeyUp](int key) {
        if (isKeyDown && eventKey == key) {
            return true;
        }
        if (isKeyUp && eventKey == key) {
            return false;
        }
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    };

    if (pressed(VK_CONTROL) || pressed(VK_LCONTROL) || pressed(VK_RCONTROL)) {
        modifiers |= ModifierCtrl;
    }
    if (pressed(VK_MENU) || pressed(VK_LMENU) || pressed(VK_RMENU)) {
        modifiers |= ModifierAlt;
    }
    if (pressed(VK_SHIFT) || pressed(VK_LSHIFT) || pressed(VK_RSHIFT)) {
        modifiers |= ModifierShift;
    }
    if (m_leftWinPhysicallyDown || m_rightWinPhysicallyDown) {
        modifiers |= ModifierWin;
    }
    m_pressedModifiers = modifiers;
    return modifiers;
}

const HotkeyRule* HotkeyHookService::matchingRule(int virtualKey, HotkeyModifiers modifiers) const {
    for (const HotkeyRule& rule : m_rules) {
        if (rule.enabled && rule.hotkey.isValid() && rule.hotkey.key == virtualKey &&
            rule.hotkey.modifiers == modifiers) {
            return &rule;
        }
    }
    return nullptr;
}

bool HotkeyHookService::isTrackedSuppressedKey(int virtualKey) const {
    return m_suppressedKeys.contains(virtualKey);
}

bool HotkeyHookService::isModifierKey(int virtualKey) const {
    switch (virtualKey) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

void HotkeyHookService::updatePressedModifierState(int virtualKey, bool pressed) {
    if (!isModifierKey(virtualKey)) {
        return;
    }

    const HotkeyModifier modifier =
        (virtualKey == VK_CONTROL || virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL) ? ModifierCtrl
        : (virtualKey == VK_MENU || virtualKey == VK_LMENU || virtualKey == VK_RMENU)        ? ModifierAlt
        : (virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT)     ? ModifierShift
                                                                                             : ModifierWin;

    if (pressed) {
        m_pressedModifiers |= modifier;
    } else {
        m_pressedModifiers &= ~modifier;
    }
}

void HotkeyHookService::trackSuppressedChord(const HotkeyCombination& hotkey) {
    if (!hotkey.isValid()) {
        return;
    }
    m_suppressedKeys.insert(hotkey.key);
}

void HotkeyHookService::clearSuppressedKey(int virtualKey) {
    m_suppressedTriggerIds.remove(virtualKey);
    m_suppressedKeys.remove(virtualKey);
}

void HotkeyHookService::resetKeyboardState(const QString& reason) {
    m_activeTriggers.clear();
    m_suppressedKeys.clear();
    m_suppressedTriggerIds.clear();
    m_pressedModifiers = ModifierNone;
    m_leftWinPhysicallyDown = false;
    m_rightWinPhysicallyDown = false;
    logHotkeyState(QString::fromLatin1("%1 reset-keyboard-state %2")
                       .arg(reason, hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown,
                                                  m_pressedModifiers)));
}
#endif
