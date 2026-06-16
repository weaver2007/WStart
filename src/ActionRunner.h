#pragma once

#include "HotkeyTypes.h"

#include <QObject>

class ActionRunner : public QObject {
    Q_OBJECT

public:
    explicit ActionRunner(QObject *parent = nullptr);
    bool run(const LaunchAction &action, QString *error = nullptr) const;
};
