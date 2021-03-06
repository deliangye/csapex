/// HEADER
#include <csapex/command/mute_node.h>

/// COMPONENT
#include <csapex/command/command.h>
#include <csapex/model/graph.h>
#include <csapex/model/node_handle.h>
#include <csapex/model/node_state.h>
#include <csapex/command/command_serializer.h>
#include <csapex/serialization/serialization_buffer.h>

/// SYSTEM
#include <sstream>

/// COMPONENT
#include <csapex/utility/assert.h>

using namespace csapex;
using namespace csapex::command;

CSAPEX_REGISTER_COMMAND_SERIALIZER(MuteNode)

MuteNode::MuteNode(const AUUID& parent_uuid, const UUID &node, bool muted)
    : CommandImplementation(parent_uuid), uuid(node), muted(muted), executed(false)
{
}

std::string MuteNode::getDescription() const
{
    return muted ? "muted" : "unmuted";
}

bool MuteNode::doExecute()
{
    NodeHandle* node_handle = getGraph()->findNodeHandle(uuid);
    apex_assert_hard(node_handle);

    bool is_muted = node_handle->getNodeState()->isMuted();

    if(muted != is_muted) {
        node_handle->getNodeState()->setMuted(muted);
        executed = true;
    } else {
        executed = false;
    }

    return true;
}

bool MuteNode::doUndo()
{
    if(executed) {
        NodeHandle* node_handle = getGraph()->findNodeHandle(uuid);
        apex_assert_hard(node_handle);

        node_handle->getNodeState()->setMuted(!muted);
    }
    return true;
}

bool MuteNode::doRedo()
{
    return doExecute();
}



void MuteNode::serialize(SerializationBuffer &data) const
{
    Command::serialize(data);

    data << uuid;
    data << muted;
    data << executed;
}

void MuteNode::deserialize(SerializationBuffer& data)
{
    Command::deserialize(data);

    data >> uuid;
    data >> muted;
    data >> executed;
}
