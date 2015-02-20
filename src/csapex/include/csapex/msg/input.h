#ifndef INPUT_H
#define INPUT_H

/// COMPONENT
#include <csapex/model/connectable.h>
#include <csapex/csapex_fwd.h>
#include <csapex/msg/message.h>
#include <csapex/utility/buffer.h>

namespace csapex
{

class Input : public Connectable
{
    Q_OBJECT

    friend class Output;
    friend class command::AddConnection;
    friend class command::MoveConnection;
    friend class command::DeleteConnection;

public:
    Input(InputTransition* transition, const UUID &uuid);
    Input(InputTransition* transition, Unique *parent, int sub_id);
    virtual ~Input();

    InputTransition* getTransition() const;

    virtual bool canInput() const {
        return true;
    }
    virtual bool isInput() const {
        return true;
    }

    void addConnection(ConnectionWeakPtr connection) override;
    void removeConnection(ConnectionWeakPtr connection) override;
    bool canConnectTo(Connectable* other_side, bool move) const;

    void inputMessage(ConnectionType::ConstPtr message);
    ConnectionTypeConstPtr getMessage() const;

    virtual bool targetsCanBeMovedTo(Connectable* other_side) const;

    virtual void connectionMovePreview(Connectable* other_side);
    virtual void validateConnections();

    Connectable* getSource() const;

    virtual CommandPtr removeAllConnectionsCmd();
    virtual void removeAllConnectionsNotUndoable();

    bool isOptional() const;
    void setOptional(bool optional);

    bool hasMessage() const;
    bool hasReceived() const;

    void free();
    void stop();

    virtual void enable();
    virtual void disable();

    virtual void notifyMessageProcessed();

    void reset();


protected:
    virtual bool isConnectionPossible(Connectable* other_side);
    virtual void removeConnection(Connectable* other_side);

protected:
    InputTransition* transition_;

    mutable std::mutex message_mutex_;
    ConnectionTypeConstPtr message_;

    bool optional_;
};

}

#endif // INPUT_H
