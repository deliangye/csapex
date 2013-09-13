/// HEADER
#include <csapex/command/delete_connection.h>

/// COMPONENT
#include <csapex/command/command.h>
#include <csapex/model/box.h>
#include <csapex/model/connector_in.h>
#include <csapex/model/connector_out.h>
#include <csapex/model/graph.h>

/// SYSTEM
#include <boost/foreach.hpp>

using namespace csapex;
using namespace csapex::command;

DeleteConnection::DeleteConnection(Connector* a, Connector* b)
{
    from = dynamic_cast<ConnectorOut*>(a);
    if(from) {
        to = dynamic_cast<ConnectorIn*>(b);
    } else {
        from = dynamic_cast<ConnectorOut*>(b);
        to = dynamic_cast<ConnectorIn*>(a);
    }
    assert(from);
    assert(to);

    from_uuid = from->UUID();
    to_uuid = to->UUID();
}

bool DeleteConnection::execute()
{
    Connection::Ptr connection(new Connection(from, to));

    Graph::Ptr graph = Graph::root();

    connection_id = graph->getConnectionId(connection);
    remove_fulcrums = graph->deleteAllConnectionFulcrumsCommand(connection);

    if(doExecute(remove_fulcrums)) {
        graph->deleteConnection(connection);
    }

    return true;
}

bool DeleteConnection::undo()
{
    if(!refresh()) {
        return false;
    }
    Graph::root()->addConnection(Connection::Ptr(new Connection(from, to, connection_id)));

    return doUndo(remove_fulcrums);
}

bool DeleteConnection::redo()
{
    if(!refresh()) {
        throw std::runtime_error("cannot redo DeleteConnection");
    }
    return execute();
}

bool DeleteConnection::refresh()
{
    Box::Ptr from_box = Graph::root()->findConnectorOwner(from_uuid);
    Box::Ptr to_box = Graph::root()->findConnectorOwner(to_uuid);

    from = NULL;
    to = NULL;

    if(!from_box || !to_box) {
        return false;
    }

    from = from_box->getOutput(from_uuid);
    to = to_box->getInput(to_uuid);

    assert(from);
    assert(to);

    return true;
}