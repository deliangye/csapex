#ifndef CONNECTOR_H
#define CONNECTOR_H

/// COMPONENT
#include <csapex/model/model_fwd.h>
#include <csapex/model/unique.h>
#include <csapex/model/error_state.h>
#include <csapex/model/connector_type.h>
#include <csapex/model/connector_description.h>
#include <csapex/csapex_export.h>

/// SYSTEM
#include <csapex/utility/slim_signal.hpp>

namespace csapex
{

class CSAPEX_EXPORT Connector : public Unique, public ErrorState, public std::enable_shared_from_this<Connector>
{
public:
    ConnectableOwnerPtr getOwner() const;

    virtual bool canConnectTo(Connector* other_side, bool move) const = 0;
    virtual bool targetsCanBeMovedTo(Connector* other_side) const = 0;
    virtual bool isConnectionPossible(Connector* other_side) = 0;
    virtual void validateConnections();
    virtual void connectionMovePreview(ConnectorPtr other_side) = 0;

    virtual int getCount() const = 0;

    virtual bool canOutput() const = 0;
    virtual bool canInput() const = 0;
    virtual bool isOutput() const = 0;
    virtual bool isInput() const = 0;
    virtual bool isOptional() const = 0;

    virtual bool isVirtual() const = 0;
    virtual void setVirtual(bool _virtual) = 0;

    virtual bool isGraphPort() const = 0;
    virtual void setGraphPort(bool graph) = 0;

    virtual bool isEssential() const = 0;
    virtual void setEssential(bool essential) = 0;

    virtual void addConnection(ConnectionPtr connection) = 0;
    virtual void removeConnection(Connector* other_side) = 0;
    virtual void fadeConnection(ConnectionPtr connection) = 0;

    virtual void setLabel(const std::string& label) = 0;
    virtual std::string getLabel() const = 0;

    virtual void setType(TokenData::ConstPtr type) = 0;
    virtual TokenData::ConstPtr getType() const = 0;

    virtual ConnectorType getConnectorType() const = 0;

    virtual ConnectorDescription getDescription() const;

    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;

    virtual int sequenceNumber() const = 0;
    virtual void setSequenceNumber(int seq_no_) = 0;

    virtual int countConnections() = 0;
    virtual std::vector<ConnectionPtr> getConnections() const = 0;

    virtual bool hasActiveConnection() const = 0;

    virtual bool isConnected() const = 0;

    virtual void disable() = 0;
    virtual void enable() = 0;

    virtual void reset() = 0;
    virtual void stop() = 0;

    virtual void notifyMessageProcessed() = 0;

public:
    slim_signal::Signal<void(bool)> enabled_changed;

    slim_signal::Signal<void()> essential_changed;

    slim_signal::Signal<void(ConnectorPtr)> disconnected;
    slim_signal::Signal<void(ConnectorPtr)> connectionStart;
    slim_signal::Signal<void(ConnectorPtr,ConnectorPtr)> connectionInProgress;

    slim_signal::Signal<void(ConnectorPtr)> connection_added_to;
    slim_signal::Signal<void(ConnectorPtr)> connection_removed_to;

    slim_signal::Signal<void(ConnectionPtr)> connection_added;
    slim_signal::Signal<void(ConnectionPtr)> connection_faded;

    slim_signal::Signal<void(bool)> connectionEnabled;
    slim_signal::Signal<void(ConnectorPtr)> message_processed;
    slim_signal::Signal<void(bool, std::string, int)> connectableError;

    slim_signal::Signal<void()> typeChanged;
    slim_signal::Signal<void(std::string)> labelChanged;

protected:
    Connector(const UUID &uuid, ConnectableOwnerWeakPtr owner);

protected:
    ConnectableOwnerWeakPtr owner_;
};

}
#endif // CONNECTOR_H
