#pragma once

#include "HotkeyTypes.h"

#include <QLineEdit>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class HotkeyEdit : public QLineEdit {
    Q_OBJECT

public:
    explicit HotkeyEdit(QWidget* parent = nullptr);
    ~HotkeyEdit() override;

    HotkeyCombination hotkey() const;
    void setHotkey(const HotkeyCombination& hotkey);

public slots:
    void beginRecording();

signals:
    void hotkeyChanged(const HotkeyCombination& hotkey);

protected:
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void startCaptureHook();
    void stopCaptureHook();
    void captureNativeHotkey(int virtualKey, HotkeyModifiers modifiers);
    void finishRecording(bool suppressCurrentChord);
    int virtualKeyFromEvent(const QKeyEvent* event) const;
    HotkeyModifiers modifiersFromEvent(const QKeyEvent* event) const;
    bool shouldCapture() const;
    HotkeyCombination m_hotkey;
    bool m_recording = false;
    bool m_suppressingCurrentChord = false;

#ifdef Q_OS_WIN
    static bool isModifierVirtualKey(int virtualKey);
    static bool hasAnyCapturedModifier();
    static HotkeyModifiers nativeModifierSnapshot();
    static void updateCapturedModifierState(int virtualKey, bool pressed);
    static LRESULT CALLBACK captureProc(int code, WPARAM wParam, LPARAM lParam);
    HHOOK m_captureHook = nullptr;
    static HotkeyEdit* s_activeEditor;
    static HotkeyModifiers s_capturedModifiers;
#endif
};
