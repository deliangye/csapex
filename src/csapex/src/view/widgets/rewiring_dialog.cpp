/// HEADER
#include <csapex/view/widgets/rewiring_dialog.h>

/// COMPONENT
#include <csapex/view/designer/graph_view.h>
#include <csapex/model/subgraph_node.h>
#include <csapex/model/graph_facade.h>
#include <csapex/scheduling/thread_pool.h>
#include <csapex/view/widgets/box_dialog.h>
#include <csapex/factory/node_factory.h>
#include <csapex/model/node_facade_local.h>
#include <csapex/model/node_handle.h>
#include <csapex/signal/slot.h>
#include <csapex/signal/event.h>
#include <csapex/msg/output.h>
#include <csapex/msg/input.h>
#include <csapex/model/connection.h>
#include <csapex/msg/direct_connection.h>
#include <csapex/param/parameter_factory.h>
#include <csapex/view/csapex_view_core_local.h>
#include <csapex/model/graph.h>

/// SYSTEM
#include <QLabel>
#include <QKeyEvent>
#include <QListView>
#include <QVBoxLayout>
#include <QDialogButtonBox>

using namespace csapex;


RewiringDialog::RewiringDialog(NodeFacade* node, CsApexViewCore& view_core, QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f),
      view_core_(view_core),

      view_core_old_(std::make_shared<CsApexViewCoreLocal>(view_core_, view_core_.getExceptionHandler())),
      view_core_new_(std::make_shared<CsApexViewCoreLocal>(view_core_, view_core_.getExceptionHandler())),
      node_facade_(node)
{
    root_uuid_provider_ = std::make_shared<UUIDProvider>();
}

RewiringDialog::~RewiringDialog()
{
}

std::string RewiringDialog::getType() const
{
    return type_new_;
}

std::vector<ConnectionInformation> RewiringDialog::getConnections(const UUID& new_node_uuid)
{
    for(SlotPtr slot_new : nh_new->getNodeHandle()->getSlots()) {
        for(ConnectionPtr connection : slot_new->getConnections()) {
            UUID to_uuid = UUIDProvider::makeDerivedUUID_forced(new_node_uuid, slot_new->getUUID().id().getFullName());
            connections_.push_back(ConnectionInformation(new_target_uuid_to_old_uuid_.at(connection->from()->getUUID()),
                                                         to_uuid,
                                                         slot_new->getType(),
                                                         connection->isActive()));
        }
    }
    for(InputPtr input_new : nh_new->getNodeHandle()->getExternalInputs()) {
        for(ConnectionPtr connection : input_new->getConnections()) {
            UUID to_uuid = UUIDProvider::makeDerivedUUID_forced(new_node_uuid, input_new->getUUID().id().getFullName());
            connections_.push_back(ConnectionInformation(new_target_uuid_to_old_uuid_.at(connection->from()->getUUID()),
                                                         to_uuid,
                                                         input_new->getType(),
                                                         connection->isActive()));
        }
    }


    for(EventPtr event_new : nh_new->getNodeHandle()->getEvents()) {
        for(ConnectionPtr connection : event_new->getConnections()) {
            UUID from_uuid = UUIDProvider::makeDerivedUUID_forced(new_node_uuid, event_new->getUUID().id().getFullName());
            connections_.push_back(ConnectionInformation(from_uuid,
                                                         new_target_uuid_to_old_uuid_.at(connection->to()->getUUID()),
                                                         event_new->getType(),
                                                         connection->isActive()));
        }
    }
    for(OutputPtr output_new : nh_new->getNodeHandle()->getExternalOutputs()) {
        for(ConnectionPtr connection : output_new->getConnections()) {
            UUID from_uuid = UUIDProvider::makeDerivedUUID_forced(new_node_uuid, output_new->getUUID().id().getFullName());
            connections_.push_back(ConnectionInformation(from_uuid,
                                                         new_target_uuid_to_old_uuid_.at(connection->to()->getUUID()),
                                                         output_new->getType(),
                                                         connection->isActive()));
        }
    }

    return connections_;
}

void RewiringDialog::makeUI(const QString& stylesheet)
{
    BoxDialog diag("Please enter the new node type.",
                   *view_core_.getNodeFactory(), *view_core_.getNodeAdapterFactory(),
                   *view_core_.getSnippetFactory());
    diag.setWindowTitle("Select new node type.");

    int r = diag.exec();

    if(!r) {
        return;
    }

    type_new_ = diag.getName();

    createGraphs(type_new_);
    createConnections();


    createUI(stylesheet);
}

void RewiringDialog::createGraphs(const std::string& type)
{
    NodeFactory& node_factory = *view_core_.getNodeFactory();

    // old graph
    graph_facade_old_ = view_core_old_->getRoot();
    apex_assert_hard(graph_facade_old_);
    graph_node_old = graph_facade_old_->getSubgraphNode();
    apex_assert_hard(graph_node_old);
    graph_old = graph_node_old->getGraph();
    apex_assert_hard(graph_old);
    graph_node_old->removeInternalPorts();

    nh_old = node_factory.makeNode(node_facade_->getType(), node_facade_->getUUID(), graph_old);
    nh_old->setNodeState(node_facade_->getNodeStateCopy());
    graph_old->addNode(nh_old);


    // new graph
    graph_facade_new_ = view_core_new_->getRoot();
    apex_assert_hard(graph_facade_new_);
    graph_node_new = graph_facade_new_->getSubgraphNode();
    apex_assert_hard(graph_node_new);
    graph_new = graph_node_new->getGraph();
    apex_assert_hard(graph_new);
    graph_node_new->removeInternalPorts();

    nh_new = node_factory.makeNode(type, graph_new->generateUUID(type), graph_new);
    graph_new->addNode(nh_new);
}

void RewiringDialog::createConnections()
{
    for(SlotPtr slot_original : node_facade_->getNodeHandle()->getSlots()) {
        for(ConnectionPtr connection : slot_original->getConnections()) {
            SlotPtr slot_old = nh_old->getNodeHandle()->getSlot(slot_original->getUUID());
            apex_assert_hard(slot_old);
            updateConnection(slot_old, connection);
        }
    }
    for(InputPtr input_original : node_facade_->getNodeHandle()->getExternalInputs()) {
        for(ConnectionPtr connection : input_original->getConnections()) {
            InputPtr input_old = nh_old->getNodeHandle()->getInput(input_original->getUUID());
            apex_assert_hard(input_old);
            updateConnection(input_old, connection);
        }
    }


    for(EventPtr event_original : node_facade_->getNodeHandle()->getEvents()) {
        for(ConnectionPtr connection : event_original->getConnections()) {
            EventPtr event_old = nh_old->getNodeHandle()->getEvent(event_original->getUUID());
            apex_assert_hard(event_old);
            updateConnection(event_old, connection);
        }
    }
    for(OutputPtr output_original : node_facade_->getNodeHandle()->getExternalOutputs()) {
        for(ConnectionPtr connection : output_original->getConnections()) {
            OutputPtr output_old = nh_old->getNodeHandle()->getOutput(output_original->getUUID());
            apex_assert_hard(output_old);
            updateConnection(output_old, connection);
        }
    }
}

void RewiringDialog::createUI(const QString& stylesheet)
{
    setStyleSheet(stylesheet);
    setWindowIcon(QIcon(":/pencil.png"));
    setWindowTitle(QString("Change Node to ") + QString::fromStdString(type_new_));

    setFocusPolicy(Qt::StrongFocus);
    setModal(true);

    QVBoxLayout* layout = new QVBoxLayout;
    setLayout(layout);

    layout->addWidget(new QLabel("Current node:"));

    GraphView* view_old = new GraphView(graph_facade_old_, *view_core_old_);
    view_old->overwriteStyleSheet(stylesheet);
    view_old->setMinimumSize(500, 350);
    view_old->setInteractive(false);
    layout->addWidget(view_old);

    layout->addWidget(new QLabel("New node:"));

    GraphView* view_new = new GraphView(graph_facade_new_, *view_core_new_);
    view_new->overwriteStyleSheet(stylesheet);
    view_new->setMinimumSize(500, 350);
    layout->addWidget(view_new);

    QDialogButtonBox* buttons = new QDialogButtonBox;
    buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(finish()));
    QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
}


void RewiringDialog::updateConnection(InputPtr input, const ConnectionPtr &connection)
{
    OutputPtr out_original = connection->from();
    UUID original_uuid = out_original->getUUID();
    UUID uuid_old;
    if(uuid_cache_.find(original_uuid) != uuid_cache_.end()) {
        uuid_old = uuid_cache_.at(original_uuid);
    } else {
        uuid_old = graph_old->generateUUID(original_uuid.id().type());
        uuid_cache_[original_uuid] = uuid_old;
        new_target_uuid_to_old_uuid_[uuid_old] = original_uuid;
    }

    UUID uuid_new = UUID::NONE;
    OutputPtr source_old = graph_old->findTypedConnectorNoThrow<Output>(uuid_old);
    OutputPtr source_new = graph_new->findTypedConnectorNoThrow<Output>(uuid_old);
    if(std::dynamic_pointer_cast<Event>(out_original)) {
        if(!source_old) {
            source_old = graph_node_old->createInternalEvent(out_original->getType(), uuid_old, out_original->getLabel());
        }
        if(!source_new) {
            source_new = graph_node_new->createInternalEvent(out_original->getType(), uuid_old, out_original->getLabel());
        }
    } else {
        if(!source_old) {
            source_old = graph_node_old->createInternalOutput(out_original->getType(), uuid_old, out_original->getLabel());
        }
        if(!source_new) {
            source_new = graph_node_new->createInternalOutput(out_original->getType(), uuid_old, out_original->getLabel());
        }
    }

    graph_old->addConnection(DirectConnection::connect(source_old, input));

    uuid_new = UUIDProvider::makeDerivedUUID_forced(nh_new->getUUID(), input->getUUID().id().getFullName());

    ConnectionPtr c;
    if(InputPtr in_new = nh_new->getNodeHandle()->getInput(uuid_new)) {
        if(!source_new->isConnectionPossible(in_new.get())) {
            return;
        }
        c = DirectConnection::connect(source_new, in_new);
    } else if(SlotPtr slot_new = nh_new->getNodeHandle()->getSlot(uuid_new)) {
        if(!source_new->isConnectionPossible(slot_new.get())) {
            return;
        }
        c = DirectConnection::connect(source_new, slot_new);
    }
    if(c) {
        c->setActive(connection->isActive());
        graph_new->addConnection(c);
    }
}

void RewiringDialog::updateConnection(OutputPtr output, const ConnectionPtr &connection)
{
    InputPtr in_original = connection->to();
    UUID uuid_old;
    UUID original_uuid = in_original->getUUID();
    if(uuid_cache_.find(original_uuid) != uuid_cache_.end()) {
        uuid_old = uuid_cache_.at(original_uuid);
    } else {
        uuid_old = graph_old->generateUUID(original_uuid.id().type());
        uuid_cache_[original_uuid] = uuid_old;
        new_target_uuid_to_old_uuid_[uuid_old] = original_uuid;
    }

    UUID uuid_new = UUID::NONE;
    InputPtr sink_old = graph_old->findTypedConnectorNoThrow<Input>(uuid_old);
    InputPtr sink_new = graph_new->findTypedConnectorNoThrow<Input>(uuid_old);
    if(std::dynamic_pointer_cast<Slot>(in_original)) {
        if(!sink_old) {
            sink_old = graph_node_old->createInternalSlot(in_original->getType(), uuid_old, in_original->getLabel(), [](const TokenPtr&){});
        }
        if(!sink_new) {
            sink_new = graph_node_new->createInternalSlot(in_original->getType(), uuid_old, in_original->getLabel(), [](const TokenPtr&){});
        }
    } else {
        if(!sink_old) {
            sink_old = graph_node_old->createInternalInput(in_original->getType(), uuid_old, in_original->getLabel(), in_original->isOptional());
        }
        if(!sink_new) {
            sink_new = graph_node_new->createInternalInput(in_original->getType(), uuid_old, in_original->getLabel(), in_original->isOptional());
        }
    }

    graph_old->addConnection(DirectConnection::connect(output, sink_old));

    uuid_new = UUIDProvider::makeDerivedUUID_forced(nh_new->getUUID(), output->getUUID().id().getFullName());

    ConnectionPtr c;
    if(OutputPtr out_new = nh_new->getNodeHandle()->getOutput(uuid_new)) {
        c = DirectConnection::connect(out_new, sink_new);
    } else if(EventPtr event_new = nh_new->getNodeHandle()->getEvent(uuid_new)) {
        c = DirectConnection::connect(event_new, sink_new);
    }

    if(c) {
        c->setActive(connection->isActive());
        graph_new->addConnection(c);
    }
}

void RewiringDialog::finish()
{
    Q_EMIT accept();
}

/// MOC
#include "../../../include/csapex/view/widgets/moc_rewiring_dialog.cpp"
