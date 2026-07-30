#pragma once

#include "HotkeyTypes.h"

#include <QHash>
#include <QSet>
#include <QVector>

// Platform-independent state owned exclusively by the keyboard-hook thread.
// Keeping matching and suppression here makes key-order regressions testable
// without installing a global Windows hook.
class HotkeyState {
public:
    void reset();
    void setModifierKey(int key, HotkeyModifier modifier, bool pressed);
    HotkeyModifiers pressedModifiers() const;

    bool isSuppressed(int key) const;
    bool isTriggerActive(const QString& triggerId) const;
    bool claimTrigger(int key, const QString& triggerId);
    QString releaseSuppressedKey(int key);

    static bool findMatchingRule(const QVector<HotkeyRule>& rules, int key, HotkeyModifiers modifiers,
                                 HotkeyRule* match = nullptr);

private:
    HotkeyModifiers m_pressedModifiers = ModifierNone;
    QHash<int, HotkeyModifier> m_pressedModifierKeys;
    QSet<int> m_suppressedKeys;
    QHash<int, QString> m_suppressedTriggerIds;
    QSet<QString> m_activeTriggers;
};
