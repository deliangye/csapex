/// HEADER
#include <csapex/command/delete_connection.h>

/// COMPONENT
#include <csapex/command/command.h>
#include <csapex/command/command_factory.h>
#include <csapex/model/node_handle.h>
#include <csapex/msg/input.h>
#include <csapex/msg/output.h>
#include <csapex/model/graph.h>
#include <csapex/model/node.h>
#include <csapex/msg/direct_connection.h>
#include <csapex/command/command_serializer.h>
#include <csapex/serialization/serialization_buffer.h>

using namespace csapex;
using namespace csapex::command;

CSAPEX_REGISTER_COMMAND_SERIALIZER(DeleteConnection)

DeleteConnection::DeleteConnection(const AUUID &parent_uuid, Connectable* a, Connectable* b)
    : Meta(parent_uuid, "delete connection and fulcrums"), active_(false), from_uuid(UUID::NONE), to_uuid(UUID::NONE)
{
    apex_assert_hard(!a->isVirtual());
    apex_assert_hard(!b->isVirtual());

    if((a->isOutput() && b->isInput())) {
        from_uuid = a->getUUID();
        to_uuid =  b->getUUID();

    } else if(a->isInput() && b->isOutput()) {
        from_uuid = b->getUUID();
        to_uuid =  a->getUUID();
    }
}

std::string DeleteConnection::getDescription() const
{
    return std::string("deleted connection between ") + from_uuid.getFullName() + " and " + to_uuid.getFullName();
}


bool DeleteConnection::doExecute()
{
    const auto& graph = getGraph();

    ConnectionPtr connection = graph->getConnection(from_uuid, to_uuid);

    apex_assert_hard(connection);

    active_ = connection->isActive();

    connection_id = connection->id();

    locked = false;
    clear();
    add(CommandFactory(getRoot(), graph_uuid).deleteAllConnectionFulcrumsCommand(connection));
    locked = true;

    if(Meta::doExecute()) {
        graph->deleteConnection(connection);
    }

    return true;
}

bool DeleteConnection::doUndo()
{
    GraphPtr graph = getGraph();

    ConnectablePtr from = graph->findConnector(from_uuid);
    ConnectablePtr to = graph->findConnector(to_uuid);

    OutputPtr output = std::dynamic_pointer_cast<Output>(from);
    InputPtr input = std::dynamic_pointer_cast<Input>(to);

    ConnectionPtr c = DirectConnection::connect(output, input, connection_id);
    c->setActive(active_);
    graph->addConnection(c);

    return Meta::doUndo();
}



bool DeleteConnection::doRedo()
{
    return doExecute();
}


void DeleteConnection::serialize(SerializationBuffer &data) const
{
    Meta::serialize(data);

    data << connection_id;
    data << active_;

    data << from_uuid;
    data << to_uuid;
}

void DeleteConnection::deserialize(SerializationBuffer& data)
{
    Meta::deserialize(data);

    data >> connection_id;
    data >>active_;

    data >> from_uuid;
    data >> to_uuid;
}
