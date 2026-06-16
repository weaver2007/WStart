#include "HotkeyEdit.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_WIN
HotkeyEdit *HotkeyEdit::s_activeEditor = nullptr;
HotkeyModifiers HotkeyEdit::s_capturedModifiers = ModifierNone;
#endif

HotkeyEdit::HotkeyEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setReadOnly(true);
    setPlaceholderText("Click here or press Record");
    setFocusPolicy(Qt::StrongFocus);
}

HotkeyEdit::~HotkeyEdit()
{
    stopCaptureHook();
}

HotkeyCombination HotkeyEdit::hotkey() const
{
    return m_hotkey;
}

void HotkeyEdit::setHotkey(const HotkeyCombination &hotkey)
{
    m_hotkey = hotkey;
    setText(m_hotkey.displayText());
}

void HotkeyEdit::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
}

void HotkeyEdit::focusOutEvent(QFocusEvent *event)
{
    m_recording = false;
    m_suppressingCurrentChord = false;
#ifdef Q_OS_WIN
    s_capturedModifiers = ModifierNone;
    if (s_activeEditor == this) {
        s_activeEditor = nullptr;
    }
#endif
    QLineEdit::focusOutEvent(event);
}

void HotkeyEdit::keyPressEvent(QKeyEvent *event)
{
    event->accept();
}

void HotkeyEdit::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    beginRecording();
    event->accept();
}

void HotkeyEdit::beginRecording()
{
    startCaptureHook();
#ifdef Q_OS_WIN
    if (!m_captureHook) {
        return;
    }
#endif

    m_recording = true;
    m_suppressingCurrentChord = false;
#ifdef Q_OS_WIN
    if (s_activeEditor && s_activeEditor != this) {
        s_activeEditor->m_recording = false;
        s_activeEditor->m_suppressingCurrentChord = false;
    }
    s_activeEditor = this;
    s_capturedModifiers = ModifierNone;
#endif
    setText("Press shortcut...");
    selectAll();
}

void HotkeyEdit::startCaptureHook()
{
#ifdef Q_OS_WIN
    if (m_captureHook) {
        return;
    }
    m_captureHook = SetWindowsHookExW(WH_KEYBOARD_LL, &HotkeyEdit::captureProc, GetModuleHandleW(nullptr), 0);
    if (!m_captureHook) {
        setText(QString("Capture hook failed: %1").arg(GetLastError()));
    }
#endif
}

void HotkeyEdit::stopCaptureHook()
{
#ifdef Q_OS_WIN
    if (m_captureHook) {
        UnhookWindowsHookEx(m_captureHook);
        m_captureHook = nullptr;
    }
    if (s_activeEditor == this) {
        s_activeEditor = nullptr;
    }
    s_capturedModifiers = ModifierNone;
#endif
}

void HotkeyEdit::captureNativeHotkey(int virtualKey, HotkeyModifiers modifiers)
{
    if (HotkeyCombination::isModifierKey(virtualKey)) {
        return;
    }

    HotkeyCombination captured;
    captured.modifiers = modifiers;
    captured.key = virtualKey;
    setHotkey(captured);
    emit hotkeyChanged(m_hotkey);
    m_recording = false;
    m_suppressingCurrentChord = true;
}

bool HotkeyEdit::shouldCapture() const
{
    return m_recording || m_suppressingCurrentChord;
}

#ifdef Q_OS_WIN
bool HotkeyEdit::isModifierVirtualKey(int virtualKey)
{
    return virtualKey == VK_CONTROL || virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL ||
           virtualKey == VK_MENU || virtualKey == VK_LMENU || virtualKey == VK_RMENU ||
           virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT ||
           virtualKey == VK_LWIN || virtualKey == VK_RWIN;
}

bool HotkeyEdit::hasAnyCapturedModifier()
{
    return s_capturedModifiers.testFlag(ModifierCtrl) ||
           s_capturedModifiers.testFlag(ModifierAlt) ||
           s_capturedModifiers.testFlag(ModifierShift) ||
           s_capturedModifiers.testFlag(ModifierWin);
}

void HotkeyEdit::updateCapturedModifierState(int virtualKey, bool pressed)
{
    HotkeyModifier modifier = ModifierNone;
    switch (virtualKey) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        modifier = ModifierCtrl;
        break;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        modifier = ModifierAlt;
        break;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        modifier = ModifierShift;
        break;
    case VK_LWIN:
    case VK_RWIN:
        modifier = ModifierWin;
        break;
    default:
        return;
    }

    if (pressed) {
        s_capturedModifiers |= modifier;
    } else {
        s_capturedModifiers &= ~modifier;
    }
}

LRESULT CALLBACK HotkeyEdit::captureProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code < 0 || !s_activeEditor || !s_activeEditor->shouldCapture()) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto *event = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
    if (!event) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const bool isKeyDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool isKeyUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
    if (!isKeyDown && !isKeyUp) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const int virtualKey = static_cast<int>(event->vkCode);
    if (isModifierVirtualKey(virtualKey)) {
        updateCapturedModifierState(virtualKey, isKeyDown);
        if (isKeyUp && !hasAnyCapturedModifier() && s_activeEditor->m_suppressingCurrentChord) {
            s_activeEditor->m_suppressingCurrentChord = false;
        }
        return 1;
    }

    if (isKeyDown && s_activeEditor->m_recording) {
        s_activeEditor->captureNativeHotkey(virtualKey, s_capturedModifiers);
    }
    return 1;
}
#endif
