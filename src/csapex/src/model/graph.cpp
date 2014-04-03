/// HEADER
#include <csapex/model/graph.h>

/// PROJECT
#include <csapex/core/settings.h>
#include <csapex/command/add_connection.h>
#include <csapex/command/add_connector.h>
#include <csapex/command/add_fulcrum.h>
#include <csapex/command/add_node.h>
#include <csapex/command/delete_connection.h>
#include <csapex/command/delete_connector.h>
#include <csapex/command/delete_fulcrum.h>
#include <csapex/command/delete_node.h>
#include <csapex/command/delete_node.h>
#include <csapex/command/dispatcher.h>
#include <csapex/command/meta.h>
#include <csapex/command/meta.h>
#include <csapex/command/move_box.h>
#include <csapex/command/move_fulcrum.h>
#include <csapex/manager/box_manager.h>
#include <csapex/model/connectable.h>
#include <csapex/model/connector_in.h>
#include <csapex/model/connector_out.h>
#include <csapex/model/node_constructor.h>
#include <csapex/model/node.h>
#include <csapex/model/node_worker.h>
#include <csapex/utility/qt_helper.hpp>
#include <csapex/utility/stream_interceptor.h>
#include <csapex/view/box.h>
#include <csapex/view/port.h>
#include "ui_designer.h"


/// SYSTEM
#include <boost/bind/protect.hpp>
#include <boost/foreach.hpp>
#include <QResizeEvent>
#include <QMenu>
#include <QScrollBar>
#include <QFileDialog>
#include <boost/algorithm/string.hpp>


using namespace csapex;

Graph::Graph(Settings& settings)
    : settings_(settings), dispatcher_(NULL)
{
    timer_ = new QTimer();
    timer_->setInterval(1000. / 30.);
    timer_->start();

    QObject::connect(timer_, SIGNAL(timeout()), this, SLOT(tick()));
}

Graph::~Graph()
{

}

Settings& Graph::getSettings() const
{
    return settings_;
}

void Graph::init(CommandDispatcher *dispatcher)
{
    dispatcher_ = dispatcher;
    assert(dispatcher_);
}

std::string Graph::makeUUIDPrefix(const std::string& name)
{
    int& last_id = uuids[name];
    ++last_id;

    std::stringstream ss;
    ss << name << "_" << last_id;

    return ss.str();
}

void Graph::addNode(Node::Ptr node)
{
    assert(!node->getUUID().empty());
    assert(!node->getType().empty());
    assert(dispatcher_);

    nodes_.push_back(node);
    node->setCommandDispatcher(dispatcher_);
    node->makeThread();

    QObject::connect(this, SIGNAL(sig_tick()), node->getNodeWorker(), SLOT(tick()));

    Q_EMIT nodeAdded(node);
}

void Graph::deleteNode(const UUID& uuid)
{
    Node* node = findNode(uuid);

    node->stop();

    QObject::disconnect(this, SIGNAL(sig_tick()), node->getNodeWorker(), SLOT(tick()));

    for(std::vector<Node::Ptr>::iterator it = nodes_.begin(); it != nodes_.end();) {
        if((*it).get() == node) {
            Q_EMIT nodeRemoved(*it);
            it = nodes_.erase(it);
        } else {
            ++it;
        }
    }
}

int Graph::countNodes()
{
    return nodes_.size();
}

int Graph::countSelectedNodes()
{
    int c = 0;

    Q_FOREACH(Node::Ptr n, nodes_) {
        if(n->getBox()->isSelected()) {
            ++c;
        }
    }

    return c;
}

int Graph::countSelectedConnections()
{
    int c = 0;

    Q_FOREACH(Connection::Ptr n, visible_connections) {
        if(n->isSelected()) {
            ++c;
        }
    }

    return c;
}


void Graph::fillContextMenuForSelection(QMenu *menu, std::map<QAction *, boost::function<void ()> > &handler)
{
    bool has_minimized = false;
    bool has_maximized = false;

    Q_FOREACH(Node::Ptr b, nodes_) {
        if(b->getBox()->isSelected()) {
            if(b->getBox()->isMinimizedSize()) {
                has_minimized = true;
            } else {
                has_maximized = true;
            }
        }
    }

    boost::function<bool(Box*)> pred_selected = boost::bind(&Box::isSelected, _1);

    if(has_minimized) {
        QAction* max = new QAction("maximize all", menu);
        max->setIcon(QIcon(":/maximize.png"));
        max->setIconVisibleInMenu(true);
        handler[max] = boost::bind(&Graph::foreachBox, this, boost::protect(boost::bind(&Box::minimizeBox, _1, false)), pred_selected);
        menu->addAction(max);
    }

    if(has_maximized){
        QAction* max = new QAction("minimize all", menu);
        max->setIcon(QIcon(":/minimize.png"));
        max->setIconVisibleInMenu(true);
        handler[max] = boost::bind(&Graph::foreachBox, this, boost::protect(boost::bind(&Box::minimizeBox, _1, true)), pred_selected);
        menu->addAction(max);
    }

    menu->addSeparator();

    QAction* term = new QAction("terminate thread", menu);
    term->setIcon(QIcon(":/stop.png"));
    term->setIconVisibleInMenu(true);
    handler[term] = boost::bind(&Graph::foreachBox, this, boost::protect(boost::bind(&Box::killContent, _1)), pred_selected);
    menu->addAction(term);

    QAction* prof = new QAction("profiling", menu);
    prof->setIcon(QIcon(":/profiling.png"));
    prof->setIconVisibleInMenu(true);
    handler[prof] = boost::bind(&Graph::foreachBox, this, boost::protect(boost::bind(&Box::showProfiling, _1)), pred_selected);
    menu->addAction(prof);

    menu->addSeparator();

    QAction* del = new QAction("delete all", menu);
    del->setIcon(QIcon(":/close.png"));
    del->setIconVisibleInMenu(true);
    handler[del] = boost::bind(&CommandDispatcher::execute, dispatcher_, boost::bind(boost::bind(&Graph::deleteSelectedNodesCmd, this)));
    menu->addAction(del);
}

void Graph::foreachNode(boost::function<void (Node*)> f, boost::function<bool (Node*)> pred)
{
    Q_FOREACH(Node::Ptr b, nodes_) {
        if(pred(b.get())) {
            f(b.get());
        }
    }
}

void Graph::foreachBox(boost::function<void (Box*)> f, boost::function<bool (Box*)> pred)
{
    Q_FOREACH(Node::Ptr b, nodes_) {
        if(pred(b->getBox())) {
            f(b->getBox());
        }
    }
}

Command::Ptr Graph::moveSelectedBoxes(const QPoint& delta)
{
    command::Meta::Ptr meta(new command::Meta("Move Selected Boxes"));

    Q_FOREACH(Node::Ptr b, nodes_) {
        if(b->getBox()->isSelected()) {
            meta->add(Command::Ptr(new command::MoveBox(b->getBox(), b->getBox()->pos())));
        }
    }

    Q_FOREACH(const Connection::Ptr& connection, visible_connections) {
        if(connection->from()->getPort()->isSelected() && connection->to()->getPort()->isSelected()) {
            int n = connection->getFulcrumCount();
            for(int i = 0; i < n; ++i) {
                const Connection::Fulcrum& f = connection->getFulcrum(i);
                meta->add(Command::Ptr(new command::MoveFulcrum(connection->id(), i, f.pos - delta, f.pos)));
            }
        }
    }

    return meta;
}

bool Graph::addConnection(Connection::Ptr connection)
{
    if(connection->from()->tryConnect(connection->to())) {
        Connectable* from = findConnector(connection->from()->getUUID());
        Connectable* to = findConnector(connection->to()->getUUID());

        visible_connections.push_back(connection);

        verify();

        Q_EMIT connectionAdded(connection.get());
        Q_EMIT from->connectionDone();
        Q_EMIT to->connectionDone();
        return true;
    }

    std::cerr << "cannot connect " << connection->from()->getUUID() << " (" <<( connection->from()->isInput() ? "i": "o" )<< ") to " << connection->to()->getUUID() << " (" <<( connection->to()->isInput() ? "i": "o" )<< ")"  << std::endl;
    return false;
}

void Graph::deleteConnection(Connection::Ptr connection)
{
    connection->from()->removeConnection(connection->to());

    for(std::vector<Connection::Ptr>::iterator c = visible_connections.begin(); c != visible_connections.end();) {
        if(*connection == **c) {
            Connectable* to = connection->to();
            to->setError(false);
            if(to->isProcessing()) {
                to->setProcessing(false);
            }
            visible_connections.erase(c);
            verify();
            Q_EMIT connectionDeleted(connection.get());
        } else {
            ++c;
        }
    }

    Q_EMIT stateChanged();
}


void Graph::verify()
{
//    Q_FOREACH(Node::Ptr node, nodes_) {
//        bool blocked = false;
//        for(int i = 0; i < node->countInputs(); ++i) {
//            blocked |= node->getInput(i)->isBlocked();
//        }

//        if(blocked) {
//            node->finishProcessing();
//        }
//    }

    verifyAsync();
}

void Graph::verifyAsync()
{
    /* Foreach node look for paths to every other node.
     * If there are two or more paths from one node to another
     *   and on of them contains an async edge, make all others
     *   temporary async
     */

    Q_FOREACH(Node::Ptr node, nodes_) {
        for(int i = 0; i < node->countInputs(); ++i) {
            node->getInput(i)->setTempAsync(false);
        }
    }

    Q_FOREACH(Node::Ptr node, nodes_) {
        std::deque<Node*> Q;
        std::map<Node*,bool> has_async_input;

        Node* current = node.get();

        Q.push_back(current);

        while(!Q.empty()) {
            Node* front = Q.front();
            Q.pop_front();

            bool visited = has_async_input.find(front) != has_async_input.end();
            if(!visited) {
                has_async_input[front] = false;
            }

            for(int i = 0; i < front->countOutputs(); ++i) {
                ConnectorOut* output = front->getOutput(i);
                for(ConnectorOut::TargetIterator in = output->beginTargets(); in != output->endTargets(); ++in) {
                    ConnectorIn* input = *in;

                    Node* next_node = findNodeForConnector(input->getUUID());

                    if(input->isAsync() || has_async_input[front]) {
                        has_async_input[next_node] = true;
                    }

                    Q.push_back(next_node);
                }
            }
        }


        for(int i = 0; i < current->countOutputs(); ++i) {
            ConnectorOut* output = current->getOutput(i);

            for(ConnectorOut::TargetIterator in = output->beginTargets(); in != output->endTargets(); ++in) {
                ConnectorIn* input = *in;

                Node* next_node = findNodeForConnector(input->getUUID());

                if(!input->isAsync()) {
                    bool a = has_async_input[next_node];
                    input->setTempAsync(a);
                }
            }
        }
    }
}

void Graph::stop()
{
    settings_.setProcessingAllowed(false);

    Q_FOREACH(Node::Ptr node, nodes_) {
        node->disable();
    }
    Q_FOREACH(Node::Ptr node, nodes_) {
        node->stop();
    }

    nodes_.clear();
}

bool Graph::isPaused() const
{
    return !timer_->isActive();
}

void Graph::setPause(bool pause)
{
    Q_FOREACH(Node::Ptr node, nodes_) {
        node->enableIO(!pause);
    }
    if(pause) {
        timer_->stop();
    } else {
        timer_->start();
    }
}


Command::Ptr Graph::clear()
{
    command::Meta::Ptr clear(new command::Meta("Clear Graph"));

    Q_FOREACH(Node::Ptr node, nodes_) {
        Command::Ptr cmd(new command::DeleteNode(node->getUUID()));
        clear->add(cmd);
    }

    return clear;
}

void Graph::reset()
{
    stop();

    settings_.setProcessingAllowed(true);

    uuids.clear();
    connectors_.clear();
    visible_connections.clear();
}

Graph::Ptr Graph::findSubGraph(const UUID& uuid)
{
    Box* bg = findNode(uuid)->getBox();
    assert(bg);
    assert(bg->hasSubGraph());

    return bg->getSubGraph();
}

Node* Graph::findNode(const UUID& uuid)
{
    Node* node = findNodeNoThrow(uuid);

    if(node) {
        return node;
    }

    std::cerr << "cannot find box \"" << uuid << "\n";
    std::cerr << "available nodes:\n";
    Q_FOREACH(Node::Ptr n, nodes_) {
        std::cerr << n->getUUID() << '\n';
    }
    std::cerr << std::endl;
    throw std::runtime_error("cannot find box");
}

Node* Graph::findNodeNoThrow(const UUID& uuid)
{
    Q_FOREACH(Node::Ptr b, nodes_) {
        if(b->getUUID() == uuid) {
            return b.get();
        }
    }

    Q_FOREACH(Node::Ptr b, nodes_) {
//        Group::Ptr grp = boost::dynamic_pointer_cast<Group> (b);
//        if(grp) {
//            Node* tmp = grp->getSubGraph()->findNodeNoThrow(uuid);
//            if(tmp) {
//                return tmp;
//            }
//        }
    }

    return NULL;
}

Node* Graph::findNodeForConnector(const UUID &uuid)
{
    UUID l = UUID::NONE;
    UUID r = UUID::NONE;
    uuid.split(UUID::namespace_separator, l, r);

    try {
        return findNode(l);

    } catch(const std::exception& e) {
        std::cerr << "error: cannot find owner of connector '" << uuid << "'\n";

        Q_FOREACH(Node::Ptr n, nodes_) {
            std::cerr << "node: " << n->getUUID() << "\n";
            std::cerr << "inputs: " << "\n";
            Q_FOREACH(ConnectorIn* in, n->input) {
                std::cerr << "\t" << in->getUUID() << "\n";
            }
            std::cerr << "outputs: " << "\n";
            Q_FOREACH(ConnectorOut* out, n->output) {
                std::cerr << "\t" << out->getUUID() << "\n";
            }
        }

        std::cerr << std::flush;

        throw std::runtime_error(std::string("cannot find owner of connector \"") + uuid.getFullName());
    }
}

Connectable* Graph::findConnector(const UUID &uuid)
{
    UUID l = UUID::NONE;
    UUID r = UUID::NONE;
    uuid.split(UUID::namespace_separator, l, r);

    Node* owner = findNode(l);
    assert(owner);

    Connectable* result = owner->getConnector(uuid);;

    assert(result);

    return result;
}

bool Graph::handleConnectionSelection(int id, bool add)
{
    if(id != -1) {
        if(add) {
            if(isConnectionWithIdSelected(id)) {
                deselectConnectionById(id);
            } else {
                selectConnectionById(id, true);
            }
        } else {
            if(isConnectionWithIdSelected(id)) {
                if(countSelectedConnections() == 1) {
                    deselectConnectionById(id);
                } else {
                    selectConnectionById(id);
                }
            } else {
                selectConnectionById(id);
            }
        }
        return false;

    } else if(!add) {
        deselectConnections();
        return false;
    }

    return true;
}

void Graph::handleNodeSelection(Node* node, bool add)
{
    if(node != NULL) {
        if(add) {
            if(node->getBox()->isSelected()) {
                node->getBox()->setSelected(false);
            } else {
                selectNode(node, true);
            }
        } else {
            if(node->getBox()->isSelected()) {
                deselectNodes();
                if(countSelectedNodes() != 1) {
                    selectNode(node->getBox()->getNode());
                }
            } else {
                selectNode(node->getBox()->getNode());
            }
        }
    } else if(!add) {
        deselectNodes();
    }
}


Connection::Ptr Graph::getConnectionWithId(int id)
{
    BOOST_FOREACH(Connection::Ptr& connection, visible_connections) {
        if(connection->id() == id) {
            return connection;
        }
    }

    return ConnectionNullPtr;
}

Connection::Ptr Graph::getConnection(Connection::Ptr c)
{
    BOOST_FOREACH(Connection::Ptr& connection, visible_connections) {
        if(*connection == *c) {
            return connection;
        }
    }

    std::cerr << "error: cannot get connection for " << *c << std::endl;

    return ConnectionNullPtr;
}

int Graph::getConnectionId(Connection::Ptr c)
{
    Connection::Ptr internal = getConnection(c);

    if(internal != ConnectionNullPtr) {
        return internal->id();
    }

    std::cerr << "error: cannot get connection id for " << *c << std::endl;

    return -1;
}

void Graph::deselectConnections()
{
    BOOST_FOREACH(Connection::Ptr& connection, visible_connections) {
        connection->setSelected(false);
    }
    Q_EMIT selectionChanged();
}

Command::Ptr Graph::deleteConnectionByIdCommand(int id)
{
    Q_FOREACH(const Connection::Ptr& connection, visible_connections) {
        if(connection->id() == id) {
            return Command::Ptr(new command::DeleteConnection(connection->from(), connection->to()));
        }
    }

    return Command::Ptr();
}

Command::Ptr Graph::deleteConnectionFulcrumCommand(int connection, int fulcrum)
{
    return Command::Ptr(new command::DeleteFulcrum(connection, fulcrum));
}

Command::Ptr Graph::deleteAllConnectionFulcrumsCommand(int connection)
{
    command::Meta::Ptr meta(new command::Meta("Delete All Connection Fulcrums"));
    int n = getConnectionWithId(connection)->getFulcrumCount();
    for(int i = n - 1; i >= 0; --i) {
        meta->add(deleteConnectionFulcrumCommand(connection, i));
    }

    return meta;
}

Command::Ptr Graph::deleteAllConnectionFulcrumsCommand(Connection::Ptr connection)
{
    return deleteAllConnectionFulcrumsCommand(getConnectionId(connection));
}


Command::Ptr Graph::deleteConnectionById(int id)
{
    Command::Ptr cmd(deleteConnectionByIdCommand(id));

    return cmd;
}

Command::Ptr Graph::deleteSelectedConnectionsCmd()
{
    command::Meta::Ptr meta(new command::Meta("Delete Selected Connections"));

    Q_FOREACH(const Connection::Ptr& connection, visible_connections) {
        if(isConnectionWithIdSelected(connection->id())) {
            meta->add(Command::Ptr(new command::DeleteConnection(connection->from(), connection->to())));
        }
    }

    deselectConnections();

    return meta;
}

void Graph::selectConnectionById(int id, bool add)
{
    if(!add) {
        BOOST_FOREACH(Connection::Ptr& connection, visible_connections) {
            connection->setSelected(false);
        }
    }
    Connection::Ptr c = getConnectionWithId(id);
    if(c != ConnectionNullPtr) {
        c->setSelected(true);
    }
    Q_EMIT selectionChanged();
}


void Graph::deselectConnectionById(int id)
{
    BOOST_FOREACH(Connection::Ptr& connection, visible_connections) {
        if(connection->id() == id) {
            connection->setSelected(false);
        }
    }
    Q_EMIT selectionChanged();
}


bool Graph::isConnectionWithIdSelected(int id)
{
    if(id < 0) {
        return false;
    }

    Q_FOREACH(const Connection::Ptr& connection, visible_connections) {
        if(connection->id() == id) {
            return connection->isSelected();
        }
    }

    std::stringstream ss; ss << "no connection with id " << id;
    throw std::runtime_error(ss.str());
}

Command::Ptr Graph::deleteSelectedNodesCmd()
{
    command::Meta::Ptr meta(new command::Meta("Delete Selected Nodes"));

    Q_FOREACH(Node::Ptr n, nodes_) {
        if(n->getBox()->isSelected()) {
            meta->add(Command::Ptr(new command::DeleteNode(n->getUUID())));
        }
    }

    deselectNodes();

    return meta;
}

void Graph::selectAll()
{
    Q_FOREACH(Node::Ptr n, nodes_) {
        n->getBox()->setSelected(true);
    }
    Q_EMIT selectionChanged();
}

void Graph::clearSelection()
{
    Q_FOREACH(Node::Ptr n, nodes_) {
        n->getBox()->setSelected(false);
    }
    Q_EMIT selectionChanged();
}

void Graph::toggleBoxSelection(Box *box)
{
    bool shift = Qt::ShiftModifier == QApplication::keyboardModifiers();

    handleNodeSelection(box->getNode(), shift);
}

void Graph::boxMoved(Box *box, int dx, int dy)
{
    if(box->isSelected() && box->hasFocus()) {
        Q_FOREACH(Node::Ptr n, nodes_) {
            Box* b = n->getBox();
            if(b != box && b->isSelected()) {
                b->move(b->x() + dx, b->y() + dy);
            }
        }
        Q_FOREACH(const Connection::Ptr& connection, visible_connections) {
            if(connection->from()->getPort()->isSelected() && connection->to()->getPort()->isSelected()) {
                int n = connection->getFulcrumCount();
                for(int i = 0; i < n; ++i) {
                    connection->moveFulcrum(i, connection->getFulcrum(i).pos + QPoint(dx,dy));
                }
            }
        }
    }
}


void Graph::deselectNodes()
{
    Q_FOREACH(Node::Ptr n, nodes_) {
        if(n->getBox()->isSelected()) {
            n->getBox()->setSelected(false);
        }
    }
    Q_EMIT selectionChanged();
}

void Graph::selectNode(Node *node, bool add)
{
    assert(!node->getBox()->isSelected());

    if(!add) {
        deselectNodes();
    }

    node->getBox()->setSelected(true);

    Q_EMIT selectionChanged();
}

void Graph::tick()
{
    //    Q_FOREACH(Node::Ptr n, nodes_) {
    //        if(n->isEnabled()) {
    //            n->tick();
    //        }
    //    }
    Q_EMIT sig_tick();

    Q_FOREACH(const Connection::Ptr& connection, visible_connections) {
        connection->tick();
    }
}
