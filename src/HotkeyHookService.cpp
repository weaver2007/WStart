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
    const HotkeyModifiers modifiers = currentModifiers(virtualKey);
    const HotkeyRule *rule = matchingRule(virtualKey, modifiers);
    if (!rule) {
        if (isKeyUp) {
            m_activeTriggers.remove(QString::number(virtualKey));
        }
        return 0;
    }

    const QString triggerId = rule->hotkey.stableId();
    if (isKeyUp) {
        m_activeTriggers.remove(triggerId);
        return 1;
    }

    if (!m_activeTriggers.contains(triggerId)) {
        m_activeTriggers.insert(triggerId);
        const HotkeyRule ruleCopy = *rule;
        QMetaObject::invokeMethod(this, [this, ruleCopy]() {
            emit hotkeyTriggered(ruleCopy);
        }, Qt::QueuedConnection);
    }
    return 1;
}

HotkeyModifiers HotkeyHookService::currentModifiers(int eventKey) const
{
    auto pressed = [eventKey](int key) {
        if (eventKey == key) {
            return true;
        }
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    };

    HotkeyModifiers modifiers = ModifierNone;
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
#endif
