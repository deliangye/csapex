#ifndef SUBGRAPH_NODE_H
#define SUBGRAPH_NODE_H

/// COMPONENT
#include <csapex/model/node.h>
#include <csapex/model/variadic_io.h>
#include <csapex/utility/uuid.h>

/// SYSTEM
#include <unordered_map>
#include <set>

namespace csapex
{

struct RelayMapping
{
    UUID external;
    UUID internal;
};


class CSAPEX_EXPORT SubgraphNode : public Node, public Variadic
{
public:
    SubgraphNode(GraphLocalPtr graph);
    ~SubgraphNode();

    GraphPtr getGraph() const;

//    template <typename T = Connectable>
//    std::shared_ptr<T> findTypedConnector(const UUID &uuid)
//    {
//        return std::dynamic_pointer_cast<T>(findConnector(uuid));
//    }
//    template <typename T = Connectable>
//    std::shared_ptr<T> findTypedConnectorNoThrow(const UUID &uuid) noexcept
//    {
//        return std::dynamic_pointer_cast<T>(findConnectorNoThrow(uuid));
//    }

//    virtual NodeHandle*findNodeHandle(const UUID& uuid) const override;
//    virtual NodeHandle* findNodeHandleNoThrow(const UUID& uuid) const noexcept override;
//    virtual ConnectablePtr findConnectorNoThrow(const UUID &uuid) noexcept override;

    virtual void initialize(csapex::NodeHandlePtr node_handle) override;
    virtual void detach() override;
    virtual void reset() override;
    virtual void stateChanged() override;
    virtual void tearDown() override;

    virtual void activation() override;
    virtual void deactivation() override;

    virtual bool canProcess() const override;
    // Node interface
    virtual void setup(csapex::NodeModifier& modifier) override;
    virtual void setupParameters(Parameterizable& params) override;
    virtual void process(csapex::NodeModifier& node_modifier, csapex::Parameterizable& params, Continuation continuation) override;

    virtual bool isAsynchronous() const override;

    InputPtr createInternalInput(const TokenDataConstPtr& type, const UUID& internal_uuid, const std::string& label, bool optional);
    OutputPtr createInternalOutput(const TokenDataConstPtr& type, const UUID& internal_uuid, const std::string& label);
    EventPtr createInternalEvent(const TokenDataConstPtr& type, const UUID& internal_uuid, const std::string& label);
    SlotPtr createInternalSlot(const TokenDataConstPtr& type, const UUID& internal_uuid, const std::string& label, std::function<void (const TokenPtr &)> callback);

    virtual Input* createVariadicInput(TokenDataConstPtr type, const std::string& label, bool optional) override;
    virtual Output* createVariadicOutput(TokenDataConstPtr type, const std::string& label) override;
    virtual Event* createVariadicEvent(TokenDataConstPtr type, const std::string& label) override;
    virtual Slot* createVariadicSlot(TokenDataConstPtr type, const std::string& label, std::function<void(const TokenPtr&)> callback, bool active, bool asynchronous) override;

    virtual void removeVariadicInput(InputPtr input) override;
    virtual void removeVariadicOutput(OutputPtr input) override;
    virtual void removeVariadicEvent(EventPtr input) override;
    virtual void removeVariadicSlot(SlotPtr input) override;

    void removeInternalPorts();

    RelayMapping addForwardingInput(const TokenDataConstPtr& type, const std::string& label, bool optional);
    RelayMapping addForwardingOutput(const TokenDataConstPtr& type, const std::string& label);
    RelayMapping addForwardingSlot(const TokenDataConstPtr& type, const std::string& label);
    RelayMapping addForwardingEvent(const TokenDataConstPtr& type, const std::string& label);

    InputPtr getForwardedInputInternal(const UUID& internal_uuid) const;
    OutputPtr getForwardedOutputInternal(const UUID& internal_uuid) const;
    SlotPtr getForwardedSlotInternal(const UUID& internal_uuid) const;
    EventPtr getForwardedEventInternal(const UUID& internal_uuid) const;

    InputPtr getForwardedInputInternalNoThrow(const UUID& internal_uuid) const noexcept;
    OutputPtr getForwardedOutputInternalNoThrow(const UUID& internal_uuid) const noexcept;
    SlotPtr getForwardedSlotInternalNoThrow(const UUID& internal_uuid) const noexcept;
    EventPtr getForwardedEventInternalNoThrow(const UUID& internal_uuid) const noexcept;

    OutputPtr getRelayForInput(const UUID& external_uuid) const;
    InputPtr getRelayForOutput(const UUID& external_uuid) const;
    EventPtr getRelayForSlot(const UUID& external_uuid) const;
    SlotPtr getRelayForEvent(const UUID& external_uuid) const;

    UUID getForwardedInputExternal(const UUID& internal_uuid) const;
    UUID getForwardedOutputExternal(const UUID& internal_uuid) const;
    UUID getForwardedSlotExternal(const UUID& internal_uuid) const;
    UUID getForwardedEventExternal(const UUID& internal_uuid) const;

    std::vector<UUID> getInternalOutputs() const;
    std::vector<UUID> getInternalInputs() const;
    std::vector<UUID> getInternalSlots() const;
    std::vector<UUID> getInternalEvents() const;


    void setIterationEnabled(const UUID& external_input_uuid, bool enabled);

    void notifyMessagesProcessed();

    std::string makeStatusString() const;

    bool isIterating() const;

private:
    UUID addForwardingInput(const UUID& internal_uuid, const TokenDataConstPtr& type, const std::string& label, bool optional);
    UUID addForwardingOutput(const UUID& internal_uuid, const TokenDataConstPtr& type, const std::string& label);
    UUID addForwardingSlot(const UUID& internal_uuid, const TokenDataConstPtr& type, const std::string& label);
    UUID addForwardingEvent(const UUID& internal_uuid, const TokenDataConstPtr& type, const std::string& label);

    void currentIterationIsProcessed();
    void subgraphHasProducedAllMessages();

    void tryFinishSubgraph();

    void finishSubgraph();
    void notifySubgraphProcessed();

    void sendCurrentIteration();

    void startNextIteration();

public:
    slim_signal::Signal<void(ConnectorPtr)> forwardingAdded;
    slim_signal::Signal<void(ConnectorPtr)> forwardingRemoved;

    slim_signal::Signal<void(ConnectorPtr,ConnectorPtr)> internalConnectionInProgress;

protected:
    GraphLocalPtr graph_;

    mutable std::recursive_mutex continuation_mutex_;
    Continuation continuation_;

    InputTransitionPtr transition_relay_in_;
    OutputTransitionPtr transition_relay_out_;

    std::unordered_map<UUID, SlotPtr, UUID::Hasher> internal_slots_;
    std::vector<UUID> internal_slot_ids_;
    std::unordered_map<UUID, EventPtr, UUID::Hasher> internal_events_;
    std::vector<UUID> internal_event_ids_;

    std::unordered_map<UUID, OutputPtr, UUID::Hasher> external_to_internal_outputs_;
    std::unordered_map<UUID, InputPtr, UUID::Hasher> external_to_internal_inputs_;
    std::unordered_map<UUID, SlotPtr, UUID::Hasher> external_to_internal_slots_;
    std::unordered_map<UUID, EventPtr, UUID::Hasher> external_to_internal_events_;

    std::unordered_map<UUID, UUID, UUID::Hasher> relay_to_external_output_;
    std::unordered_map<UUID, UUID, UUID::Hasher> relay_to_external_input_;
    std::unordered_map<UUID, UUID, UUID::Hasher> relay_to_external_slot_;
    std::unordered_map<UUID, UUID, UUID::Hasher> relay_to_external_event_;

    std::set<UUID> iterated_inputs_;
    param::BitSetParameterPtr iterated_inputs_param_;
    bool is_subgraph_finished_;
    bool is_iterating_;
    bool has_sent_current_iteration_;
    int iteration_index_;
    int iteration_count_;

    bool is_initialized_;

    EventPtr activation_event_;
    EventPtr deactivation_event_;

    long guard_;
};

}

#endif // SUBGRAPH_NODE_H
