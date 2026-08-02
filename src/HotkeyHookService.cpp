#include "HotkeyHookService.h"

#include "AppLogger.h"

#include <QMetaType>
#include <QMutexLocker>
#include <QThread>

#ifdef Q_OS_WIN
HotkeyHookService* HotkeyHookService::s_instance = nullptr;

namespace {
const ULONG_PTR kCancelStartMenuExtraInfo = static_cast<ULONG_PTR>(0x57535457u);
const UINT kResetHookStateMessage = WM_APP + 0x257;
const DWORD kHookStartupTimeoutMs = 10000;
const UINT kWinStateWatchdogIntervalMs = 100;
const quint32 kForwardedWinStaleTimeoutMs = 1500;
const quint32 kClaimedWinStaleTimeoutMs = 500;

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
    case VK_CONTROL:
        return QString::fromLatin1("VK_CONTROL");
    case VK_SHIFT:
        return QString::fromLatin1("VK_SHIFT");
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

bool isPhysicalKeyDown(int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

struct SystemWinReleaseResult {
    UINT sentInputCount = 0;
    bool winKeyUpSent = false;
    DWORD error = ERROR_SUCCESS;
};

UINT sendTaggedKeyboardInput(WORD virtualKey, DWORD flags, DWORD* error) {
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtualKey;
    input.ki.dwFlags = flags;
    input.ki.dwExtraInfo = kCancelStartMenuExtraInfo;

    SetLastError(ERROR_SUCCESS);
    const UINT sent = SendInput(1, &input, sizeof(INPUT));
    if (sent != 1 && error && *error == ERROR_SUCCESS) {
        *error = GetLastError();
    }
    return sent;
}

SystemWinReleaseResult sendSystemWinKeyUp(int winVirtualKey) {
    SystemWinReleaseResult result;
    // Windows opens Start when it sees Win-down followed directly by Win-up.
    // F24 is first sent as a harmless chord member, then the Win lifecycle is
    // explicitly closed. The helper up is issued independently even if its
    // down fails, so a partial SendInput result cannot leave F24 pressed.
    result.sentInputCount += sendTaggedKeyboardInput(VK_F24, 0, &result.error);
    result.sentInputCount += sendTaggedKeyboardInput(VK_F24, KEYEVENTF_KEYUP, &result.error);
    result.winKeyUpSent = sendTaggedKeyboardInput(static_cast<WORD>(winVirtualKey),
                                                  KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY, &result.error) == 1;
    result.sentInputCount += result.winKeyUpSent ? 1u : 0u;
    return result;
}
} // namespace

class HotkeyHookThread : public QThread {
public:
    explicit HotkeyHookThread(HotkeyHookService* service)
        : m_service(service), m_readyEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {
        if (!m_readyEvent) {
            m_startupError = GetLastError();
        }
    }

    ~HotkeyHookThread() override {
        if (m_readyEvent) {
            CloseHandle(m_readyEvent);
        }
    }

    bool waitUntilReady(DWORD timeoutMs) const {
        return m_readyEvent && WaitForSingleObject(m_readyEvent, timeoutMs) == WAIT_OBJECT_0;
    }

    bool canStart() const {
        return m_readyEvent != nullptr;
    }

    DWORD startupError() const {
        return m_startupError;
    }

    void requestStop() const {
        InterlockedExchange(&m_stopRequested, 1);
        const LONG threadId = InterlockedCompareExchange(&m_threadId, 0, 0);
        if (threadId != 0) {
            PostThreadMessageW(static_cast<DWORD>(threadId), WM_QUIT, 0, 0);
        }
    }

    void requestStateReset() const {
        const LONG threadId = InterlockedCompareExchange(&m_threadId, 0, 0);
        if (threadId != 0) {
            PostThreadMessageW(static_cast<DWORD>(threadId), kResetHookStateMessage, 0, 0);
        }
    }

protected:
    void run() override {
        InterlockedExchange(&m_threadId, static_cast<LONG>(GetCurrentThreadId()));

        // Creating the queue before signalling readiness guarantees that stop()
        // can always wake this thread with PostThreadMessage.
        MSG message;
        PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

        HotkeyHookService::s_instance = m_service;
        m_service->resetKeyboardState(QString::fromLatin1("hook-thread-start"));
        m_service->releaseStaleSystemWinKeysAtStartup();
        if (!m_service->isPaused()) {
            m_service->synchronizePhysicalModifierState();
        }
        m_service->m_hook =
            SetWindowsHookExW(WH_KEYBOARD_LL, &HotkeyHookService::keyboardProc, GetModuleHandleW(nullptr), 0);
        UINT_PTR watchdogTimer = 0;
        if (!m_service->m_hook) {
            m_startupError = GetLastError();
        } else {
            InterlockedExchange(&m_service->m_running, 1);
            watchdogTimer = SetTimer(nullptr, 0, kWinStateWatchdogIntervalMs, nullptr);
            if (watchdogTimer == 0) {
                m_service->queueDiagnostic(
                    QString::fromLatin1("win-watchdog-start-failed error=%1").arg(GetLastError()));
            }
        }
        SetEvent(m_readyEvent);

        if (m_service->m_hook && InterlockedCompareExchange(&m_stopRequested, 0, 0) == 0) {
            while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                if (message.message == kResetHookStateMessage) {
                    m_service->releaseForwardedWinKeys(QString::fromLatin1("hook-state-reset"), GetTickCount());
                    m_service->resetKeyboardState(QString::fromLatin1("hook-state-reset"));
                    if (!m_service->isPaused()) {
                        m_service->synchronizePhysicalModifierState();
                    }
                    continue;
                }
                if (watchdogTimer != 0 && message.message == WM_TIMER && message.wParam == watchdogTimer) {
                    if (!m_service->isPaused()) {
                        m_service->recoverStaleWinKeys(GetTickCount());
                    }
                    continue;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }

        if (watchdogTimer != 0) {
            KillTimer(nullptr, watchdogTimer);
        }
        m_service->releaseForwardedWinKeys(QString::fromLatin1("hook-thread-stop"), GetTickCount());
        if (m_service->m_hook) {
            UnhookWindowsHookEx(m_service->m_hook);
            m_service->m_hook = nullptr;
        }

        m_service->resetKeyboardState(QString::fromLatin1("hook-thread-stop"));
        InterlockedExchange(&m_service->m_running, 0);
        if (HotkeyHookService::s_instance == m_service) {
            HotkeyHookService::s_instance = nullptr;
        }
    }

private:
    HotkeyHookService* m_service = nullptr;
    HANDLE m_readyEvent = nullptr;
    mutable volatile LONG m_threadId = 0;
    mutable volatile LONG m_stopRequested = 0;
    DWORD m_startupError = ERROR_SUCCESS;
};
#endif

HotkeyHookService::HotkeyHookService(QObject* parent) : QObject(parent) {
    qRegisterMetaType<HotkeyRule>("HotkeyRule");
    connect(this, SIGNAL(queuedHotkeyTriggered(HotkeyRule)), this, SIGNAL(hotkeyTriggered(HotkeyRule)),
            Qt::QueuedConnection);
    connect(this, SIGNAL(diagnosticQueued(QString)), this, SLOT(writeQueuedDiagnostic(QString)), Qt::QueuedConnection);
}

HotkeyHookService::~HotkeyHookService() {
    stop();
}

bool HotkeyHookService::start(QString* error) {
#ifdef Q_OS_WIN
    if (isRunning()) {
        return true;
    }
    if (m_thread) {
        stop();
    }

    m_thread = new HotkeyHookThread(this);
    if (!m_thread->canStart()) {
        const DWORD startupError = m_thread->startupError();
        delete m_thread;
        m_thread = nullptr;
        if (error) {
            *error = QString::fromLatin1("Unable to create keyboard hook synchronization event (Windows error %1).")
                         .arg(startupError);
        }
        queueDiagnostic(QString::fromLatin1("hook-ready-event-failed error=%1").arg(startupError));
        return false;
    }
    m_thread->start();
    if (!m_thread->waitUntilReady(kHookStartupTimeoutMs)) {
        m_thread->requestStop();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
        if (error) {
            *error = QString::fromLatin1("Keyboard hook thread did not start within %1 ms.").arg(kHookStartupTimeoutMs);
        }
        queueDiagnostic(QString::fromLatin1("hook-thread-start-timeout"));
        return false;
    }

    const DWORD startupError = m_thread->startupError();
    if (startupError != ERROR_SUCCESS || !isRunning()) {
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
        if (error) {
            *error = QString::fromLatin1("SetWindowsHookEx failed with error %1.").arg(startupError);
        }
        queueDiagnostic(QString::fromLatin1("hook-start-failed error=%1").arg(startupError));
        return false;
    }

    queueDiagnostic(
        QString::fromLatin1("hook-started thread=dedicated %1")
            .arg(hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_state.pressedModifiers())));
    return true;
#else
    Q_UNUSED(error)
    // Non-Windows platforms keep the UI and rule management available first.
    return true;
#endif
}

void HotkeyHookService::stop() {
#ifdef Q_OS_WIN
    if (!m_thread) {
        return;
    }
    m_thread->requestStop();
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
    queueDiagnostic(QString::fromLatin1("hook-stopped"));
#endif
}

bool HotkeyHookService::isRunning() const {
#ifdef Q_OS_WIN
    return m_running != 0;
#else
    return false;
#endif
}

void HotkeyHookService::setPaused(bool paused) {
#ifdef Q_OS_WIN
    const bool wasPaused = InterlockedExchange(&m_paused, paused ? 1 : 0) != 0;
    if (wasPaused == paused) {
        return;
    }
    queueDiagnostic(QString::fromLatin1("set-paused paused=%1").arg(boolText(paused)));
    if (m_thread) {
        m_thread->requestStateReset();
    }
#else
    m_paused = paused;
#endif
}

bool HotkeyHookService::isPaused() const {
#ifdef Q_OS_WIN
    return m_paused != 0;
#else
    return m_paused;
#endif
}

void HotkeyHookService::setRules(const QVector<HotkeyRule>& rules) {
    QMutexLocker locker(&m_rulesMutex);
    m_rules = rules;
}

void HotkeyHookService::writeQueuedDiagnostic(const QString& message) {
    AppLogger::writeLine(QString::fromLatin1("HOTKEY"), message);
}

#ifdef Q_OS_WIN
LRESULT CALLBACK HotkeyHookService::keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && s_instance) {
        const KBDLLHOOKSTRUCT* event = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        const LRESULT result = s_instance->handleKeyboardEvent(wParam, event);
        if (result != 0) {
            return result;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT HotkeyHookService::handleKeyboardEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* event) {
    if (isPaused() || !event) {
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
    if (isWinKey(virtualKey)) {
        const bool wasPhysicallyDown = m_winState.isPhysicallyDown(virtualKey);
        WinKeyState::EventDisposition disposition = WinKeyState::EventDisposition::Forward;
        if (isKeyDown) {
            disposition = m_winState.handleKeyDown(virtualKey, event->time);
            if (disposition == WinKeyState::EventDisposition::Forward) {
                updatePressedModifierState(virtualKey, true);
            }
        } else {
            disposition = m_winState.handleKeyUp(virtualKey, event->time);
            updatePressedModifierState(virtualKey, false);
        }
        synchronizeWinStateFlags();
        queueDiagnostic(
            QString::fromLatin1("win-event event=%1 key=%2 repeat=%3 disposition=%4 flags=%5 injected=%6 %7")
                .arg(eventName(isKeyDown, isKeyUp), keyName(virtualKey))
                .arg(boolText(isKeyDown && wasPhysicallyDown), disposition == WinKeyState::EventDisposition::Suppress
                                                                   ? QString::fromLatin1("suppress")
                                                                   : QString::fromLatin1("forward"))
                .arg(flagsText(event->flags), boolText((event->flags & LLKHF_INJECTED) != 0),
                     hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_state.pressedModifiers())));
        if (disposition == WinKeyState::EventDisposition::Suppress) {
            return 1;
        }
    } else if (isModifier) {
        updatePressedModifierState(virtualKey, isKeyDown);
    }

    // Once a chord is claimed, swallow the trigger key-up too. Win events are
    // handled above because claimed Win lifecycles have already supplied the
    // system key-up and must consume the later physical release.
    if (isTrackedSuppressedKey(virtualKey)) {
        queueDiagnostic(
            QString::fromLatin1("swallow-suppressed event=%1 key=%2 %3")
                .arg(eventName(isKeyDown, isKeyUp), keyName(virtualKey),
                     hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_state.pressedModifiers())));
        if (isKeyUp) {
            const QString finishedTrigger = m_state.releaseSuppressedKey(virtualKey);
            if (!finishedTrigger.isEmpty()) {
                const QVector<int> claimedWinKeys = m_winState.claimedPressedKeys();
                for (int winKey : claimedWinKeys) {
                    m_state.setModifierKey(winKey, ModifierWin, false);
                }
                if (!claimedWinKeys.isEmpty()) {
                    queueDiagnostic(QString::fromLatin1("trigger-finished trigger=%1 clearedClaimedWinModifiers=%2")
                                        .arg(finishedTrigger)
                                        .arg(claimedWinKeys.size()));
                }
            }
        }
        return 1;
    }

    // A release belongs to Windows unless this hook previously claimed the
    // corresponding key-down. Swallowing an untracked release can leave the
    // foreground application believing that the key is still held.
    if (!isKeyDown) {
        return 0;
    }

    const HotkeyModifiers modifiers = currentModifiers(virtualKey, isKeyDown, isKeyUp);
    HotkeyRule rule;
    if (!matchingRule(virtualKey, modifiers, &rule)) {
        return 0;
    }

    const QString triggerId = rule.hotkey.stableId();
    if (m_state.claimTrigger(rule.hotkey.key, triggerId)) {
        queueDiagnostic(
            QString::fromLatin1("trigger-start rule=%1 hotkey=%2 key=%3 modifiers=%4 %5")
                .arg(rule.id, rule.hotkey.displayText(), keyName(virtualKey))
                .arg(static_cast<int>(modifiers))
                .arg(hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_state.pressedModifiers())));

        if (rule.hotkey.modifiers.testFlag(ModifierWin)) {
            const QVector<int> releaseKeys = m_winState.claimPressedKeys(event->time);
            const QVector<int> claimedWinKeys = m_winState.claimedPressedKeys();
            for (int winKey : claimedWinKeys) {
                // The trigger key is already tracked independently, so keeping
                // ModifierWin active after a successful match only creates a
                // window in which unrelated input can inherit Win.
                m_state.setModifierKey(winKey, ModifierWin, false);
            }
            synchronizeWinStateFlags();
            for (int winKey : releaseKeys) {
                const SystemWinReleaseResult releaseResult = sendSystemWinKeyUp(winKey);
                if (!releaseResult.winKeyUpSent) {
                    m_winState.markSystemReleaseFailed(winKey, GetTickCount());
                }
                queueDiagnostic(QString::fromLatin1("claim-win-key trigger=%1 key=%2 sent=%3 winUp=%4 error=%5")
                                    .arg(triggerId, keyName(winKey))
                                    .arg(releaseResult.sentInputCount)
                                    .arg(boolText(releaseResult.winKeyUpSent))
                                    .arg(releaseResult.error));
            }
            queueDiagnostic(QString::fromLatin1("claimed-win-modifiers-cleared trigger=%1 count=%2")
                                .arg(triggerId)
                                .arg(claimedWinKeys.size()));
            synchronizeWinStateFlags();
        }
        emit queuedHotkeyTriggered(rule);
    }
    return 1;
}

HotkeyModifiers HotkeyHookService::currentModifiers(int eventKey, bool isKeyDown, bool isKeyUp) {
    Q_UNUSED(eventKey)
    Q_UNUSED(isKeyDown)
    Q_UNUSED(isKeyUp)
    return m_state.pressedModifiers();
}

bool HotkeyHookService::matchingRule(int virtualKey, HotkeyModifiers modifiers, HotkeyRule* match) const {
    QMutexLocker locker(&m_rulesMutex);
    return HotkeyState::findMatchingRule(m_rules, virtualKey, modifiers, match);
}

bool HotkeyHookService::isTrackedSuppressedKey(int virtualKey) const {
    return m_state.isSuppressed(virtualKey);
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
    m_state.setModifierKey(virtualKey, modifier, pressed);
}

void HotkeyHookService::resetKeyboardState(const QString& reason) {
    m_state.reset();
    m_winState.reset();
    m_leftWinPhysicallyDown = false;
    m_rightWinPhysicallyDown = false;
    queueDiagnostic(
        QString::fromLatin1("%1 reset-keyboard-state %2")
            .arg(reason, hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_state.pressedModifiers())));
}

void HotkeyHookService::synchronizePhysicalModifierState() {
    const quint32 eventTime = GetTickCount();
    const bool leftWinDown = isPhysicalKeyDown(VK_LWIN);
    const bool rightWinDown = isPhysicalKeyDown(VK_RWIN);
    m_winState.synchronizeKey(VK_LWIN, leftWinDown, eventTime);
    m_winState.synchronizeKey(VK_RWIN, rightWinDown, eventTime);
    synchronizeWinStateFlags();

    const auto synchronizeModifier = [this](int genericKey, int leftKey, int rightKey, HotkeyModifier modifier) {
        const bool leftDown = isPhysicalKeyDown(leftKey);
        const bool rightDown = isPhysicalKeyDown(rightKey);
        m_state.setModifierKey(leftKey, modifier, leftDown);
        m_state.setModifierKey(rightKey, modifier, rightDown);
        // Some remote-input and accessibility providers report only the
        // generic virtual key. Track it only when neither side is available.
        m_state.setModifierKey(genericKey, modifier, !leftDown && !rightDown && isPhysicalKeyDown(genericKey));
    };
    synchronizeModifier(VK_CONTROL, VK_LCONTROL, VK_RCONTROL, ModifierCtrl);
    synchronizeModifier(VK_MENU, VK_LMENU, VK_RMENU, ModifierAlt);
    synchronizeModifier(VK_SHIFT, VK_LSHIFT, VK_RSHIFT, ModifierShift);
    m_state.setModifierKey(VK_LWIN, ModifierWin, m_leftWinPhysicallyDown);
    m_state.setModifierKey(VK_RWIN, ModifierWin, m_rightWinPhysicallyDown);
    queueDiagnostic(
        QString::fromLatin1("physical-state-synchronized %1")
            .arg(hookStateText(m_leftWinPhysicallyDown, m_rightWinPhysicallyDown, m_state.pressedModifiers())));
}

void HotkeyHookService::recoverStaleWinKeys(quint32 currentTime) {
    const QVector<WinKeyRecoveryAction> actions =
        m_winState.expireStaleKeys(currentTime, kForwardedWinStaleTimeoutMs, kClaimedWinStaleTimeoutMs);
    for (const WinKeyRecoveryAction& action : actions) {
        m_state.setModifierKey(action.key, ModifierWin, false);

        SystemWinReleaseResult releaseResult;
        if (action.releaseSystemKey) {
            releaseResult = sendSystemWinKeyUp(action.key);
            if (!releaseResult.winKeyUpSent) {
                m_winState.markSystemReleaseFailed(action.key, currentTime);
                m_state.setModifierKey(action.key, ModifierWin, true);
            }
        }

        queueDiagnostic(QString::fromLatin1("stale-win-recovered key=%1 releaseSystem=%2 sent=%3 winUp=%4 error=%5")
                            .arg(keyName(action.key), boolText(action.releaseSystemKey))
                            .arg(releaseResult.sentInputCount)
                            .arg(boolText(releaseResult.winKeyUpSent))
                            .arg(releaseResult.error));
    }
    if (!actions.isEmpty()) {
        synchronizeWinStateFlags();
    }
}

void HotkeyHookService::releaseForwardedWinKeys(const QString& reason, quint32 currentTime) {
    const QVector<int> releaseKeys = m_winState.claimForwardedKeys(currentTime);
    for (int winKey : releaseKeys) {
        const SystemWinReleaseResult releaseResult = sendSystemWinKeyUp(winKey);
        if (!releaseResult.winKeyUpSent) {
            m_winState.markSystemReleaseFailed(winKey, currentTime);
        }
        queueDiagnostic(QString::fromLatin1("%1 release-forwarded-win key=%2 sent=%3 winUp=%4 error=%5")
                            .arg(reason, keyName(winKey))
                            .arg(releaseResult.sentInputCount)
                            .arg(boolText(releaseResult.winKeyUpSent))
                            .arg(releaseResult.error));
    }
    synchronizeWinStateFlags();
}

void HotkeyHookService::releaseStaleSystemWinKeysAtStartup() {
    const int winKeys[] = {VK_LWIN, VK_RWIN};
    for (int winKey : winKeys) {
        if (!isPhysicalKeyDown(winKey)) {
            continue;
        }
        const SystemWinReleaseResult releaseResult = sendSystemWinKeyUp(winKey);
        queueDiagnostic(QString::fromLatin1("startup-win-recovery key=%1 sent=%2 winUp=%3 error=%4")
                            .arg(keyName(winKey))
                            .arg(releaseResult.sentInputCount)
                            .arg(boolText(releaseResult.winKeyUpSent))
                            .arg(releaseResult.error));
    }
}

void HotkeyHookService::synchronizeWinStateFlags() {
    m_leftWinPhysicallyDown = m_winState.isPhysicallyDown(VK_LWIN);
    m_rightWinPhysicallyDown = m_winState.isPhysicallyDown(VK_RWIN);
}

void HotkeyHookService::queueDiagnostic(const QString& message) {
    emit diagnosticQueued(message);
}
#endif
