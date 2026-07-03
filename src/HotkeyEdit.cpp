#include "HotkeyEdit.h"

#include "AppLogger.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_WIN
HotkeyEdit* HotkeyEdit::s_activeEditor = nullptr;
HotkeyModifiers HotkeyEdit::s_capturedModifiers = ModifierNone;
namespace {
const ULONG_PTR kHotkeyEditSyntheticInputExtraInfo = static_cast<ULONG_PTR>(0x57535445u);

bool isHotkeyEditSyntheticInput(const KBDLLHOOKSTRUCT* event) {
    return event && (event->flags & LLKHF_INJECTED) && event->dwExtraInfo == kHotkeyEditSyntheticInputExtraInfo;
}

bool isWinVirtualKey(int virtualKey) {
    return virtualKey == VK_LWIN || virtualKey == VK_RWIN;
}

QString captureBoolText(bool value) {
    return value ? QString::fromLatin1("1") : QString::fromLatin1("0");
}

QString captureKeyName(int virtualKey) {
    switch (virtualKey) {
    case VK_LWIN:
        return QString::fromLatin1("VK_LWIN");
    case VK_RWIN:
        return QString::fromLatin1("VK_RWIN");
    default:
        return QString::fromLatin1("VK_0x%1").arg(virtualKey, 0, 16).toUpper();
    }
}

QString captureEventName(bool isKeyDown, bool isKeyUp) {
    if (isKeyDown) {
        return QString::fromLatin1("down");
    }
    if (isKeyUp) {
        return QString::fromLatin1("up");
    }
    return QString::fromLatin1("other");
}

void logCaptureState(const QString& message) {
    AppLogger::writeLine(QString::fromLatin1("HOTKEY_EDIT"), message);
}

void sendSyntheticKeyUp(WORD virtualKey) {
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtualKey;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    if (isWinVirtualKey(virtualKey)) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    input.ki.dwExtraInfo = kHotkeyEditSyntheticInputExtraInfo;
    SendInput(1, &input, sizeof(INPUT));
}

void sendSyntheticWinKeyUps() {
    sendSyntheticKeyUp(VK_LWIN);
    sendSyntheticKeyUp(VK_RWIN);
}

} // namespace
#endif
HotkeyEdit::HotkeyEdit(QWidget* parent) : QLineEdit(parent) {
    setReadOnly(true);
    setPlaceholderText("Click here or press Record");
    setFocusPolicy(Qt::StrongFocus);
}

HotkeyEdit::~HotkeyEdit() {
    stopCaptureHook();
}

HotkeyCombination HotkeyEdit::hotkey() const {
    return m_hotkey;
}

void HotkeyEdit::setHotkey(const HotkeyCombination& hotkey) {
    m_hotkey = hotkey;
    setText(m_hotkey.displayText());
}

void HotkeyEdit::focusInEvent(QFocusEvent* event) {
    QLineEdit::focusInEvent(event);
}

void HotkeyEdit::focusOutEvent(QFocusEvent* event) {
    finishRecording(false);
#ifdef Q_OS_WIN
    sendSyntheticWinKeyUps();
    logCaptureState(QString::fromLatin1("focus-out synthetic-win-up reset capturedMods=%1")
                        .arg(static_cast<int>(s_capturedModifiers)));
    s_capturedModifiers = ModifierNone;
    if (s_activeEditor == this) {
        s_activeEditor = nullptr;
    }
#endif
    QLineEdit::focusOutEvent(event);
}

void HotkeyEdit::keyPressEvent(QKeyEvent* event) {
    if (m_recording) {
        const int virtualKey = virtualKeyFromEvent(event);
        if (virtualKey > 0 && !HotkeyCombination::isModifierKey(virtualKey)) {
            const HotkeyModifiers modifiers = modifiersFromEvent(event);
            captureNativeHotkey(virtualKey, modifiers);
        }
    }
    event->accept();
}

void HotkeyEdit::keyReleaseEvent(QKeyEvent* event) {
    const Qt::KeyboardModifiers activeModifiers =
        event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    if (m_suppressingCurrentChord && activeModifiers == Qt::NoModifier) {
        m_suppressingCurrentChord = false;
    }
    event->accept();
}

void HotkeyEdit::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    beginRecording();
    event->accept();
}

void HotkeyEdit::beginRecording() {
    setFocus(Qt::ShortcutFocusReason);
    startCaptureHook();

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

void HotkeyEdit::startCaptureHook() {
#ifdef Q_OS_WIN
    if (m_captureHook) {
        return;
    }
    m_captureHook = SetWindowsHookExW(WH_KEYBOARD_LL, &HotkeyEdit::captureProc, GetModuleHandleW(nullptr), 0);
    if (!m_captureHook) {
        const DWORD lastError = GetLastError();
        logCaptureState(QString::fromLatin1("capture-hook-start-failed error=%1").arg(lastError));
        setText(QString("Capture hook failed: %1").arg(lastError));
    } else {
        logCaptureState(QString::fromLatin1("capture-hook-started"));
    }
#endif
}

void HotkeyEdit::stopCaptureHook() {
#ifdef Q_OS_WIN
    sendSyntheticWinKeyUps();
    logCaptureState(QString::fromLatin1("capture-hook-stop synthetic-win-up capturedMods=%1 hookActive=%2")
                        .arg(static_cast<int>(s_capturedModifiers))
                        .arg(captureBoolText(m_captureHook != nullptr)));
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

void HotkeyEdit::captureNativeHotkey(int virtualKey, HotkeyModifiers modifiers) {
    if (HotkeyCombination::isModifierKey(virtualKey)) {
        return;
    }

    HotkeyCombination captured;
    captured.modifiers = modifiers;
    captured.key = virtualKey;
#ifdef Q_OS_WIN
    logCaptureState(QString::fromLatin1("capture-hotkey key=%1 modifiers=%2")
                        .arg(captureKeyName(virtualKey))
                        .arg(static_cast<int>(modifiers)));
#endif
    setHotkey(captured);
    emit hotkeyChanged(m_hotkey);
    finishRecording(true);
}

void HotkeyEdit::finishRecording(bool suppressCurrentChord) {
    m_recording = false;
    m_suppressingCurrentChord = suppressCurrentChord;
}

int HotkeyEdit::virtualKeyFromEvent(const QKeyEvent* event) const {
    if (!event) {
        return 0;
    }
    const quint32 nativeKey = event->nativeVirtualKey();
    if (nativeKey > 0) {
        return static_cast<int>(nativeKey);
    }

    const int key = event->key();
#ifdef Q_OS_WIN
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return 'A' + key - Qt::Key_A;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return '0' + key - Qt::Key_0;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return VK_F1 + key - Qt::Key_F1;
    }
    switch (key) {
    case Qt::Key_Space:
        return VK_SPACE;
    case Qt::Key_Tab:
        return VK_TAB;
    case Qt::Key_Escape:
        return VK_ESCAPE;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return VK_RETURN;
    case Qt::Key_Backspace:
        return VK_BACK;
    case Qt::Key_Delete:
        return VK_DELETE;
    case Qt::Key_Insert:
        return VK_INSERT;
    case Qt::Key_Home:
        return VK_HOME;
    case Qt::Key_End:
        return VK_END;
    case Qt::Key_PageUp:
        return VK_PRIOR;
    case Qt::Key_PageDown:
        return VK_NEXT;
    case Qt::Key_Left:
        return VK_LEFT;
    case Qt::Key_Right:
        return VK_RIGHT;
    case Qt::Key_Up:
        return VK_UP;
    case Qt::Key_Down:
        return VK_DOWN;
    case Qt::Key_Minus:
        return VK_OEM_MINUS;
    case Qt::Key_Equal:
        return VK_OEM_PLUS;
    case Qt::Key_BracketLeft:
        return VK_OEM_4;
    case Qt::Key_BracketRight:
        return VK_OEM_6;
    case Qt::Key_Backslash:
        return VK_OEM_5;
    case Qt::Key_Semicolon:
        return VK_OEM_1;
    case Qt::Key_Apostrophe:
        return VK_OEM_7;
    case Qt::Key_Comma:
        return VK_OEM_COMMA;
    case Qt::Key_Period:
        return VK_OEM_PERIOD;
    case Qt::Key_Slash:
        return VK_OEM_2;
    case Qt::Key_QuoteLeft:
        return VK_OEM_3;
    default:
        break;
    }
#endif
    return 0;
}

HotkeyModifiers HotkeyEdit::modifiersFromEvent(const QKeyEvent* event) const {
    HotkeyModifiers modifiers = ModifierNone;
    if (!event) {
        return modifiers;
    }
    const Qt::KeyboardModifiers qtModifiers = event->modifiers();
    if (qtModifiers.testFlag(Qt::ControlModifier)) {
        modifiers |= ModifierCtrl;
    }
    if (qtModifiers.testFlag(Qt::AltModifier)) {
        modifiers |= ModifierAlt;
    }
    if (qtModifiers.testFlag(Qt::ShiftModifier)) {
        modifiers |= ModifierShift;
    }
    if (qtModifiers.testFlag(Qt::MetaModifier)) {
        modifiers |= ModifierWin;
    }
#ifdef Q_OS_WIN
    modifiers |= nativeModifierSnapshot();
#endif
    return modifiers;
}

bool HotkeyEdit::shouldCapture() const {
    return m_recording || m_suppressingCurrentChord;
}

#ifdef Q_OS_WIN
bool HotkeyEdit::isModifierVirtualKey(int virtualKey) {
    return virtualKey == VK_CONTROL || virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL ||
           virtualKey == VK_MENU || virtualKey == VK_LMENU || virtualKey == VK_RMENU || virtualKey == VK_SHIFT ||
           virtualKey == VK_LSHIFT || virtualKey == VK_RSHIFT || virtualKey == VK_LWIN || virtualKey == VK_RWIN;
}

bool HotkeyEdit::hasAnyCapturedModifier() {
    return s_capturedModifiers.testFlag(ModifierCtrl) || s_capturedModifiers.testFlag(ModifierAlt) ||
           s_capturedModifiers.testFlag(ModifierShift) || s_capturedModifiers.testFlag(ModifierWin);
}

HotkeyModifiers HotkeyEdit::nativeModifierSnapshot() {
    auto pressed = [](int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };

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

void HotkeyEdit::updateCapturedModifierState(int virtualKey, bool pressed) {
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

LRESULT CALLBACK HotkeyEdit::captureProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0 || !s_activeEditor || !s_activeEditor->shouldCapture()) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const auto* event = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (!event) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (isHotkeyEditSyntheticInput(event)) {
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
        if (isWinVirtualKey(virtualKey)) {
            logCaptureState(QString::fromLatin1("capture-win-modifier event=%1 key=%2 flags=0x%3 capturedMods=%4")
                                .arg(captureEventName(isKeyDown, isKeyUp), captureKeyName(virtualKey),
                                     QString::number(static_cast<qulonglong>(event->flags), 16).toUpper())
                                .arg(static_cast<int>(s_capturedModifiers)));
        }
        if (isKeyUp && isWinVirtualKey(virtualKey)) {
            sendSyntheticKeyUp(static_cast<WORD>(virtualKey));
            logCaptureState(QString::fromLatin1("capture-win-keyup-synthetic key=%1 capturedMods=%2")
                                .arg(captureKeyName(virtualKey))
                                .arg(static_cast<int>(s_capturedModifiers)));
        }
        if (isKeyUp && !hasAnyCapturedModifier() && s_activeEditor->m_suppressingCurrentChord) {
            s_activeEditor->m_suppressingCurrentChord = false;
        }
        return 1;
    }

    if (isKeyDown && s_activeEditor->m_recording) {
        s_activeEditor->captureNativeHotkey(virtualKey, s_capturedModifiers | nativeModifierSnapshot());
    }
    if (isKeyUp && s_activeEditor->m_suppressingCurrentChord && !hasAnyCapturedModifier()) {
        s_activeEditor->m_suppressingCurrentChord = false;
    }
    return 1;
}
#endif
