#include "HotkeyHookService.h"

#include <QCoreApplication>
#include <QMetaObject>

#ifdef Q_OS_WIN
HotkeyHookService *HotkeyHookService::s_instance = nullptr;
#endif

HotkeyHookService::HotkeyHookService(QObject *parent)
    : QObject(parent)
{
}

HotkeyHookService::~HotkeyHookService()
{
    stop();
}

bool HotkeyHookService::start(QString *error)
{
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
    if (error) {
        *error = "Global keyboard hooks are implemented for Windows only.";
    }
    return false;
#endif
}

void HotkeyHookService::stop()
{
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

bool HotkeyHookService::isRunning() const
{
#ifdef Q_OS_WIN
    return m_hook != nullptr;
#else
    return false;
#endif
}

void HotkeyHookService::setPaused(bool paused)
{
    m_paused = paused;
    if (paused) {
        m_activeTriggers.clear();
#ifdef Q_OS_WIN
        m_suppressedKeys.clear();
        m_suppressedTriggerIds.clear();
        m_pressedModifiers = ModifierNone;
        m_interceptingWinChord = false;
#endif
    }
}

bool HotkeyHookService::isPaused() const
{
    return m_paused;
}

void HotkeyHookService::setRules(const QVector<HotkeyRule> &rules)
{
    m_rules = rules;
}

#ifdef Q_OS_WIN
LRESULT CALLBACK HotkeyHookService::keyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && s_instance) {
        const auto *event = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        const LRESULT result = s_instance->handleKeyboardEvent(wParam, event);
        if (result != 0) {
            return result;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT HotkeyHookService::handleKeyboardEvent(WPARAM wParam, const KBDLLHOOKSTRUCT *event)
{
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
    const bool isWin = isWinKey(virtualKey);

    if (isKeyDown && isModifier) {
        updatePressedModifierState(virtualKey, true);
    }
    if (isKeyUp && isModifier) {
        updatePressedModifierState(virtualKey, false);
    }

    if (isTrackedSuppressedKey(virtualKey)) {
        if (isKeyUp) {
            const QString triggerId = m_suppressedTriggerIds.take(virtualKey);
            if (!triggerId.isEmpty()) {
                m_activeTriggers.remove(triggerId);
            }
            clearSuppressedKey(virtualKey);
            if (isWin && !winModifierPressed()) {
                m_interceptingWinChord = false;
            }
        }
        return 1;
    }

    if (isWin && isKeyDown && hasEnabledWinHotkey()) {
        m_interceptingWinChord = true;
        return 1;
    }

    if (m_interceptingWinChord && isModifier) {
        if (isModifier && isKeyUp) {
            updatePressedModifierState(virtualKey, false);
            if (!winModifierPressed()) {
                m_interceptingWinChord = false;
            }
        }
        return 1;
    }

    const HotkeyModifiers modifiers = currentModifiers(virtualKey);
    const HotkeyRule *rule = matchingRule(virtualKey, modifiers);
    if (!rule) {
        if (m_interceptingWinChord) {
            if (isWin && isKeyUp && !winModifierPressed()) {
                m_interceptingWinChord = false;
            }
            return 1;
        }
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
        trackSuppressedChord(rule->hotkey);
        m_suppressedTriggerIds.insert(rule->hotkey.key, triggerId);
        const HotkeyRule ruleCopy = *rule;
        QMetaObject::invokeMethod(this, [this, ruleCopy]() {
            emit hotkeyTriggered(ruleCopy);
        }, Qt::QueuedConnection);
    }
    if (isModifier) {
        updatePressedModifierState(virtualKey, true);
    }
    return 1;
}

HotkeyModifiers HotkeyHookService::currentModifiers(int eventKey) const
{
    HotkeyModifiers modifiers = m_pressedModifiers;

    if (eventKey == VK_CONTROL || eventKey == VK_LCONTROL || eventKey == VK_RCONTROL) {
        modifiers |= ModifierCtrl;
    }
    if (eventKey == VK_MENU || eventKey == VK_LMENU || eventKey == VK_RMENU) {
        modifiers |= ModifierAlt;
    }
    if (eventKey == VK_SHIFT || eventKey == VK_LSHIFT || eventKey == VK_RSHIFT) {
        modifiers |= ModifierShift;
    }
    if (eventKey == VK_LWIN || eventKey == VK_RWIN) {
        modifiers |= ModifierWin;
    }

    auto pressed = [eventKey](int key) {
        if (eventKey == key) {
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
    return modifiers;
}

const HotkeyRule *HotkeyHookService::matchingRule(int virtualKey, HotkeyModifiers modifiers) const
{
    for (const HotkeyRule &rule : m_rules) {
        if (rule.enabled && rule.hotkey.isValid() && rule.hotkey.key == virtualKey && rule.hotkey.modifiers == modifiers) {
            return &rule;
        }
    }
    return nullptr;
}

bool HotkeyHookService::isTrackedSuppressedKey(int virtualKey) const
{
    return m_suppressedKeys.contains(virtualKey);
}

bool HotkeyHookService::isModifierKey(int virtualKey) const
{
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

bool HotkeyHookService::isWinKey(int virtualKey) const
{
    return virtualKey == VK_LWIN || virtualKey == VK_RWIN;
}

bool HotkeyHookService::hasEnabledWinHotkey() const
{
    for (const HotkeyRule &rule : m_rules) {
        if (rule.enabled && rule.hotkey.isValid() && rule.hotkey.modifiers.testFlag(ModifierWin)) {
            return true;
        }
    }
    return false;
}

void HotkeyHookService::updatePressedModifierState(int virtualKey, bool pressed)
{
    if (!isModifierKey(virtualKey)) {
        return;
    }

    const HotkeyModifier modifier =
        (virtualKey == VK_CONTROL || virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL) ? ModifierCtrl :
        (virtualKey == VK_MENU || virtualKey == VK_LMENU || virtualKey == VK_RMENU) ? ModifierAlt :
        (virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT) ? ModifierShift :
        ModifierWin;

    if (pressed) {
        m_pressedModifiers |= modifier;
    } else {
        m_pressedModifiers &= ~modifier;
    }
}

bool HotkeyHookService::winModifierPressed() const
{
    return m_pressedModifiers.testFlag(ModifierWin);
}

void HotkeyHookService::trackSuppressedChord(const HotkeyCombination &hotkey)
{
    if (!hotkey.isValid()) {
        return;
    }
    m_suppressedKeys.insert(hotkey.key);
    if (hotkey.modifiers.testFlag(ModifierCtrl)) {
        m_suppressedKeys.insert(VK_CONTROL);
        m_suppressedKeys.insert(VK_LCONTROL);
        m_suppressedKeys.insert(VK_RCONTROL);
    }
    if (hotkey.modifiers.testFlag(ModifierAlt)) {
        m_suppressedKeys.insert(VK_MENU);
        m_suppressedKeys.insert(VK_LMENU);
        m_suppressedKeys.insert(VK_RMENU);
    }
    if (hotkey.modifiers.testFlag(ModifierShift)) {
        m_suppressedKeys.insert(VK_SHIFT);
        m_suppressedKeys.insert(VK_LSHIFT);
        m_suppressedKeys.insert(VK_RSHIFT);
    }
    if (hotkey.modifiers.testFlag(ModifierWin)) {
        m_interceptingWinChord = true;
        m_suppressedKeys.insert(VK_LWIN);
        m_suppressedKeys.insert(VK_RWIN);
    }
}

void HotkeyHookService::clearSuppressedKey(int virtualKey)
{
    m_suppressedTriggerIds.remove(virtualKey);
    m_suppressedKeys.remove(virtualKey);
    switch (virtualKey) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        m_suppressedKeys.remove(VK_CONTROL);
        m_suppressedKeys.remove(VK_LCONTROL);
        m_suppressedKeys.remove(VK_RCONTROL);
        break;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        m_suppressedKeys.remove(VK_MENU);
        m_suppressedKeys.remove(VK_LMENU);
        m_suppressedKeys.remove(VK_RMENU);
        break;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        m_suppressedKeys.remove(VK_SHIFT);
        m_suppressedKeys.remove(VK_LSHIFT);
        m_suppressedKeys.remove(VK_RSHIFT);
        break;
    case VK_LWIN:
    case VK_RWIN:
        m_suppressedKeys.remove(VK_LWIN);
        m_suppressedKeys.remove(VK_RWIN);
        break;
    default:
        break;
    }
}
#endif
