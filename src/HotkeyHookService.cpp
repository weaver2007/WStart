#include "HotkeyHookService.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QMetaType>

#ifdef Q_OS_WIN
HotkeyHookService* HotkeyHookService::s_instance = nullptr;
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
    s_instance = this;
    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, &HotkeyHookService::keyboardProc, GetModuleHandleW(nullptr), 0);
    if (!m_hook) {
        if (error) {
            *error = QString("SetWindowsHookEx failed with error %1.").arg(GetLastError());
        }
        s_instance = nullptr;
        return false;
    }
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
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    if (s_instance == this) {
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
    if (paused) {
        m_activeTriggers.clear();
#ifdef Q_OS_WIN
        m_suppressedKeys.clear();
        m_suppressedTriggerIds.clear();
        m_pressedModifiers = ModifierNone;
#endif
    }
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

    const bool isKeyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool isKeyUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!isKeyDown && !isKeyUp) {
        return 0;
    }

    const int virtualKey = static_cast<int>(event->vkCode);
    const bool isModifier = isModifierKey(virtualKey);

    if (isKeyDown && isModifier) {
        updatePressedModifierState(virtualKey, true);
    }
    if (isKeyUp && isModifier) {
        updatePressedModifierState(virtualKey, false);
    }

    // Once a chord is claimed, swallow the matching key-up events too. Letting
    // the release through is enough for Windows to complete some Win-key system shortcuts.
    if (isTrackedSuppressedKey(virtualKey)) {
        if (isKeyUp) {
            const QString triggerId = m_suppressedTriggerIds.take(virtualKey);
            if (!triggerId.isEmpty()) {
                m_activeTriggers.remove(triggerId);
            }
            clearSuppressedKey(virtualKey);
        }
        return 1;
    }

    const HotkeyModifiers modifiers = currentModifiers(virtualKey, isKeyDown);
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
        return 1;
    }

    if (!m_activeTriggers.contains(triggerId)) {
        m_activeTriggers.insert(triggerId);
        // The low-level hook sees each key as a separate event; remember the
        // non-modifier key so repeated key-down messages do not re-launch the action.
        trackSuppressedChord(rule->hotkey);
        m_suppressedTriggerIds.insert(rule->hotkey.key, triggerId);
        const HotkeyRule ruleCopy = *rule;
        emit queuedHotkeyTriggered(ruleCopy);
    }
    if (isModifier) {
        updatePressedModifierState(virtualKey, true);
    }
    return 1;
}

HotkeyModifiers HotkeyHookService::currentModifiers(int eventKey, bool isKeyDown) {
    HotkeyModifiers modifiers = ModifierNone;

    auto pressed = [eventKey, isKeyDown](int key) {
        if (isKeyDown && eventKey == key) {
            return true;
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
    if (pressed(VK_LWIN) || pressed(VK_RWIN)) {
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
#endif
