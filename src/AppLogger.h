#pragma once

#include <QString>

class AppLogger {
public:
    static void install();
    static void shutdown();
    static bool isInstalled();
    static QString logFilePath();
    static void writeLine(const QString& level, const QString& message);
};
