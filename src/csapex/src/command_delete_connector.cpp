/// HEADER
#include <csapex/command_delete_connector.h>

/// COMPONENT
#include <csapex/box.h>
#include <csapex/connector_in.h>
#include <csapex/connector_out.h>
#include <csapex/graph.h>
#include <csapex/command_dispatcher.h>

using namespace csapex;
using namespace command;

DeleteConnector::DeleteConnector(Connector *_c) :
    in(_c->isInput()),
    c(_c)
{
    assert(c);
    c_uuid = c->UUID();

    graph = _c->getBox()->getGraph();
}

bool DeleteConnector::execute()
{
    Box* box_c = graph->findConnectorOwner(c_uuid);

    if(c->isConnected()) {
        if(in) {
            delete_connections = ((ConnectorIn*) c)->removeAllConnectionsCmd();
        } else {
            delete_connections = ((ConnectorOut*) c)->removeAllConnectionsCmd();
        }
        CommandDispatcher::execute(delete_connections);
    }

    if(in) {
        box_c->removeInput((ConnectorIn*) c);
    } else {
        box_c->removeOutput((ConnectorOut*) c);
    }

    return true;
}

bool DeleteConnector::undo()
{
    if(!refresh()) {
        return false;
    }

    return false;
}

bool DeleteConnector::redo()
{
    return false;
}

bool DeleteConnector::refresh()
{
    Box* box_c = graph->findConnectorOwner(c_uuid);

    if(!box_c) {
        return false;
    }

    if(in) {
        c = box_c->getInput(c_uuid);
    } else {
        c = box_c->getOutput(c_uuid);
    }

    assert(c);

    return true;
}


