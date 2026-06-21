#pragma once

#include <QString>

class SelfUpdater {
public:
    static bool isPortableMode();
    static bool startPortableUpdate(const QString& packagePath, QString* error = nullptr);
};
