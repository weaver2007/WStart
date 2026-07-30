#include "SingleInstanceServer.h"

#include <QLocalSocket>

SingleInstanceServer::SingleInstanceServer(QObject* parent) : QLocalServer(parent) {
    connect(this, SIGNAL(newConnection()), this, SLOT(drainPendingConnections()));
}

void SingleInstanceServer::drainPendingConnections() {
    bool receivedActivation = false;
    while (hasPendingConnections()) {
        QLocalSocket* socket = nextPendingConnection();
        if (!socket) {
            continue;
        }
        receivedActivation = true;
        socket->disconnectFromServer();
        socket->deleteLater();
    }
    if (receivedActivation) {
        emit activationRequested();
    }
}
