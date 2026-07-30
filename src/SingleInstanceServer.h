#pragma once

#include <QLocalServer>

class SingleInstanceServer : public QLocalServer {
    Q_OBJECT

public:
    explicit SingleInstanceServer(QObject* parent = nullptr);

signals:
    void activationRequested();

private slots:
    void drainPendingConnections();
};
