/// HEADER
#include <csapex/command/flip_sides.h>

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

CSAPEX_REGISTER_COMMAND_SERIALIZER(FlipSides)

FlipSides::FlipSides(const AUUID& parent_uuid, const UUID &node)
    : CommandImplementation(parent_uuid), uuid(node)
{
}

std::string FlipSides::getDescription() const
{
    std::stringstream ss;
    ss << "flipped sides of " << uuid;
    return ss.str();
}

bool FlipSides::doExecute()
{
    NodeHandle* node_handle = getGraph()->findNodeHandle(uuid);
    apex_assert_hard(node_handle);

    bool flip = !node_handle->getNodeState()->isFlipped();
    node_handle->getNodeState()->setFlipped(flip);

    return true;
}

bool FlipSides::doUndo()
{
    return doExecute();
}

bool FlipSides::doRedo()
{
    return doExecute();
}



void FlipSides::serialize(SerializationBuffer &data) const
{
    Command::serialize(data);

    data << uuid;
}

void FlipSides::deserialize(SerializationBuffer& data)
{
    Command::deserialize(data);

    data >> uuid;
}
