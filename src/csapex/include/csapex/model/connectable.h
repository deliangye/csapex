#ifndef CONNECTABLE_H
#define CONNECTABLE_H

/// COMPONENT
#include <csapex/model/model_fwd.h>
#include <csapex/model/token_data.h>
#include <csapex/model/connector.h>
#include <csapex/csapex_export.h>

/// SYSTEM
#include <mutex>
#include <vector>
#include <atomic>
#include <csapex/utility/slim_signal.hpp>
#include <memory>

namespace csapex
{

class CSAPEX_EXPORT Connectable : public Connector
{
    friend class Graph;
    friend class Connection;

public:
    virtual ~Connectable();

    int getCount() const;

    virtual bool canConnectTo(Connector* other_side, bool move) const;

    virtual bool canOutput() const {
        return false;
    }
    virtual bool canInput() const {
        return false;
    }
    virtual bool isOutput() const {
        return false;
    }
    virtual bool isInput() const {
        return false;
    }    
    virtual bool isOptional() const {
        return false;
    }

    bool isVirtual() const;
    void setVirtual(bool _virtual);

    bool isGraphPort() const;
    void setGraphPort(bool graph);

    bool isEssential() const;
    void setEssential(bool essential);

    virtual void addConnection(ConnectionPtr connection);
    virtual void removeConnection(Connector* other_side);
    virtual void fadeConnection(ConnectionPtr connection);

    void setLabel(const std::string& label);
    std::string getLabel() const;

    void setType(TokenData::ConstPtr type);
    TokenData::ConstPtr getType() const;

    virtual ConnectorType getConnectorType() const = 0;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    int sequenceNumber() const;
    void setSequenceNumber(int seq_no_);

    int countConnections();
    std::vector<ConnectionPtr> getConnections() const;

    bool hasEnabledConnection() const;
    bool hasActiveConnection() const;

    virtual bool isConnected() const;

    virtual void disable();
    virtual void enable();

    virtual void reset();
    virtual void stop();

    virtual void notifyMessageProcessed();

protected:
    virtual void removeAllConnectionsNotUndoable() = 0;

protected:
    Connectable(const UUID &uuid, ConnectableOwnerWeakPtr owner);

    void init();

    void errorEvent(bool error, const std::string &msg, ErrorLevel level) override;


protected:
    mutable std::recursive_mutex io_mutex;
    mutable std::recursive_mutex sync_mutex;

    std::string label_;

    TokenData::ConstPtr type_;
    std::vector<ConnectionPtr> connections_;

    std::atomic<int> count_;
    std::atomic<int> seq_no_;

    bool virtual_;
    bool graph_port_;
    bool essential_;

private:
    std::atomic<bool> enabled_;
};

}

#endif // CONNECTABLE_H
