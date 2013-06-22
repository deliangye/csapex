#ifndef CONNECTOR_IN_H
#define CONNECTOR_IN_H

/// COMPONENT
#include "connector.h"

namespace vision_evaluator
{

/// FORWARDS DECLARATION
class ConnectorOut;

namespace command {
class AddConnection;
class DeleteConnection;
}

class ConnectorIn : public Connector
{
    Q_OBJECT

    friend class ConnectorOut;
    friend class command::AddConnection;
    friend class command::DeleteConnection;

public:
    ConnectorIn(Box* parent, int sub_id);
    ~ConnectorIn();

    virtual bool isInput() {
        return true;
    }

    virtual void inputMessage(ConnectionType::Ptr message);
    virtual ConnectionType::Ptr getMessage();

    virtual bool canConnect();
    virtual bool isConnected();

    virtual Command::Ptr removeAllConnectionsCmd();

private:
    virtual bool tryConnect(Connector* other_side);
    virtual bool acknowledgeConnection(Connector* other_side);
    virtual void removeConnection(Connector* other_side);
    virtual void removeAllConnectionsNotUndoable();

Q_SIGNALS:
    void messageArrived(ConnectorIn* source);

private:
    ConnectorOut* input;

    ConnectionType::Ptr message_;
};

}

#endif // CONNECTOR_IN_H
