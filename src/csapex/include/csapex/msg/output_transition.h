#ifndef OUTPUT_TRANSITION_H
#define OUTPUT_TRANSITION_H

/// COMPONENT
#include <csapex/msg/transition.h>
#include <csapex/utility/uuid.h>

/// SYSTEM
#include <unordered_map>

namespace csapex
{
class OutputTransition : public Transition
{
public:
    OutputTransition(delegate::Delegate0<> activation_fn);
    OutputTransition();

    OutputPtr getOutput(const UUID& id) const;

    void addOutput(OutputPtr output);
    void removeOutput(OutputPtr output);

    void setSequenceNumber(long seq_no);
    long getSequenceNumber() const;

    void connectionAdded(Connection* connection) override;
    void connectionRemoved(Connection *connection) override;

    bool isSink() const;
    bool canStartSendingMessages() const;
    void sendMessages();
    void publishNextMessage();

    void startReceiving();
    void abortSendingMessages();
    void setConnectionsReadyToReceive();
    bool areOutputsIdle() const;

    virtual bool isEnabled() const override;

    virtual void reset() override;

    void establishConnections() override;

public:
    csapex::slim_signal::Signal<void()> messages_processed;

private:
    void fillConnections();

private:
    std::unordered_map<OutputPtr, std::vector<csapex::slim_signal::Connection>> output_signal_connections_;
    std::unordered_map<UUID, OutputPtr, UUID::Hasher> outputs_;

    long sequence_number_;
};
}

#endif // OUTPUT_TRANSITION_H

