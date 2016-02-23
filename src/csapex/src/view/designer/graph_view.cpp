/// HEADER
#include <csapex/view/designer/graph_view.h>

/// COMPONENT
#include <csapex/command/add_node.h>
#include <csapex/command/command_factory.h>
#include <csapex/command/create_thread.h>
#include <csapex/command/delete_node.h>
#include <csapex/command/disable_node.h>
#include <csapex/command/dispatcher.h>
#include <csapex/command/flip_sides.h>
#include <csapex/command/group_nodes.h>
#include <csapex/command/meta.h>
#include <csapex/command/minimize.h>
#include <csapex/command/move_box.h>
#include <csapex/command/move_connection.h>
#include <csapex/command/paste_graph.h>
#include <csapex/command/switch_thread.h>
#include <csapex/command/set_color.h>
#include <csapex/core/graphio.h>
#include <csapex/core/settings.h>
#include <csapex/factory/node_factory.h>
#include <csapex/model/graph_facade.h>
#include <csapex/model/graph.h>
#include <csapex/model/node.h>
#include <csapex/model/node_handle.h>
#include <csapex/model/node_worker.h>
#include <csapex/msg/input.h>
#include <csapex/msg/output.h>
#include <csapex/scheduling/thread_group.h>
#include <csapex/scheduling/thread_pool.h>
#include <csapex/signal/slot.h>
#include <csapex/signal/trigger.h>
#include <csapex/view/designer/designer_scene.h>
#include <csapex/view/designer/drag_io.h>
#include <csapex/view/node/box.h>
#include <csapex/view/node/note_box.h>
#include <csapex/view/node/default_node_adapter.h>
#include <csapex/view/node/node_adapter_factory.h>
#include <csapex/view/node/node_adapter.h>
#include <csapex/view/utility/clipboard.h>
#include <csapex/view/utility/context_menu_handler.h>
#include <csapex/view/utility/node_list_generator.h>
#include <csapex/view/widgets/box_dialog.h>
#include <csapex/view/widgets/message_preview_widget.h>
#include <csapex/view/widgets/movable_graphics_proxy_widget.h>
#include <csapex/view/widgets/port.h>
#include <csapex/view/widgets/profiling_widget.h>
#include <csapex/view/widgets/port_panel.h>

/// SYSTEM
#include <iostream>
#include <QKeyEvent>
#include <QApplication>
#include <QShortcut>
#include <QGraphicsItem>
#include <QGLWidget>
#include <QInputDialog>
#include <QScrollBar>
#include <QDrag>
#include <QMimeData>
#include <QColorDialog>

using namespace csapex;

GraphView::GraphView(DesignerScene *scene, GraphFacadePtr graph_facade,
                     Settings &settings, NodeFactory& node_factory, NodeAdapterFactory& node_adapter_factory,
                     CommandDispatcher *dispatcher, DragIO& dragio, DesignerStyleable *style,
                     Designer *parent)
    : QGraphicsView(parent), parent_(parent), scene_(scene), style_(style), settings_(settings),
      node_factory_(node_factory), node_adapter_factory_(node_adapter_factory),
      graph_facade_(graph_facade), dispatcher_(dispatcher), drag_io_(dragio),
      scalings_to_perform_(0), middle_mouse_dragging_(false), move_event_(nullptr),
      preview_widget_(nullptr)

{
    //    setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers))); // memory leak?
    QGLFormat fmt;
    fmt.setSampleBuffers(true);
    fmt.setSamples(2);
    setViewport(new QGLWidget(fmt));

    setCacheMode(QGraphicsView::CacheBackground);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    setScene(scene_);
    setFocusPolicy(Qt::StrongFocus);
    setFocus(Qt::OtherFocusReason);

    setAcceptDrops(true);

    setDragMode(QGraphicsView::RubberBandDrag);
    setInteractive(true);

    QShortcut *box_shortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Space), this);
    QObject::connect(box_shortcut, SIGNAL(activated()), this, SLOT(showBoxDialog()));

    QShortcut *box_reset_view = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_0), this);
    QObject::connect(box_reset_view, SIGNAL(activated()), this, SLOT(resetZoom()));

    QShortcut *box_zoom_in = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Plus), this);
    QObject::connect(box_zoom_in, SIGNAL(activated()), this, SLOT(zoomIn()));

    QShortcut *box_zoom_out = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Minus), this);
    QObject::connect(box_zoom_out, SIGNAL(activated()), this, SLOT(zoomOut()));

    QObject::connect(scene_, SIGNAL(selectionChanged()), this, SLOT(updateSelection()));
    QObject::connect(scene_, SIGNAL(selectionChanged()), this, SIGNAL(selectionChanged()));

    //    QObject::connect(&scalings_animation_timer_, SIGNAL(timeout()), this, SLOT(animateZoom()));
    QObject::connect(&scroll_animation_timer_, SIGNAL(timeout()), this, SLOT(animateScroll()));

    QObject::connect(this, SIGNAL(startProfilingRequest(NodeWorker*)), this, SLOT(startProfiling(NodeWorker*)));
    QObject::connect(this, SIGNAL(stopProfilingRequest(NodeWorker*)), this, SLOT(stopProfiling(NodeWorker*)));

    setContextMenuPolicy(Qt::DefaultContextMenu);


    QObject::connect(this, SIGNAL(triggerConnectorCreated(ConnectablePtr)), this, SLOT(connectorCreated(ConnectablePtr)));
    QObject::connect(this, SIGNAL(triggerConnectorRemoved(ConnectablePtr)), this, SLOT(connectorRemoved(ConnectablePtr)));

    qRegisterMetaType < ConnectablePtr > ("ConnectablePtr");

    scoped_connections_.emplace_back(graph_facade_->nodeWorkerAdded.connect([this](NodeWorkerPtr n) { nodeAdded(n); }));
    scoped_connections_.emplace_back(graph_facade_->nodeRemoved.connect([this](NodeHandlePtr n) { nodeRemoved(n); }));

    Graph* graph = graph_facade_->getGraph();

    for(auto it = graph->beginNodes(); it != graph->endNodes(); ++it) {
        const NodeHandlePtr& nh = *it;
        apex_assert_hard(nh.get());
        NodeWorkerPtr nw = graph_facade_->getNodeWorker(nh.get());
        apex_assert_hard(nw);
        nodeAdded(nw);
    }

    relayed_outputs_widget_ = new PortPanel(graph_facade_, PortPanel::Type::OUTPUT, scene_);
    relayed_outputs_widget_proxy_ = scene_->addWidget(relayed_outputs_widget_);

    relayed_inputs_widget_ = new PortPanel(graph_facade_, PortPanel::Type::INPUT, scene_);
    relayed_inputs_widget_proxy_ = scene_->addWidget(relayed_inputs_widget_);
}

GraphView::~GraphView()
{
    handle_connections_.clear();
    worker_connections_.clear();

    delete scene_;
}

void GraphView::paintEvent(QPaintEvent *e)
{
    if(!scene_->isEmpty()) {
        QPointF tl_view = mapToScene(QPoint(0, 0));
        QPointF br_view = mapToScene(QPoint(viewport()->width(), viewport()->height()));

        QPointF mid = 0.5 * (tl_view + br_view);

        {
            QPointF pos(tl_view.x(),
                        mid.y() - relayed_outputs_widget_->height() / 2.0);
            if(pos != relayed_outputs_widget_proxy_->pos()) {
                relayed_outputs_widget_proxy_->setPos(pos);
            }
        }
        {
            QPointF pos(br_view.x()-relayed_inputs_widget_->width(),
                        mid.y() - relayed_inputs_widget_->height() / 2.0);
            if(pos != relayed_inputs_widget_proxy_->pos()) {
                relayed_inputs_widget_proxy_->setPos(pos);
            }
        }
    }
    QGraphicsView::paintEvent(e);

    Q_EMIT viewChanged();
}

void GraphView::resizeEvent(QResizeEvent *event)
{
    scene_->setSceneRect(scene_->itemsBoundingRect());

    QGraphicsView::resizeEvent(event);
}

void GraphView::scrollContentsBy(int dx, int dy)
{
    QRectF min_r = scene_->itemsBoundingRect();

    QPointF tl_view = mapToScene(QPoint(0, 0));
    QPointF br_view = mapToScene(QPoint(width(), height()));

    double mx = std::abs(dx * 5) + 10;
    double my = std::abs(dy * 5) + 10;

    QPointF tl(std::min(tl_view.x() - mx, min_r.x()),
               std::min(tl_view.y() - my, min_r.y()));
    QPointF br(std::max(br_view.x() + mx, min_r.x() + min_r.width()),
               std::max(br_view.y() + my, min_r.y() + min_r.height()));

    QRectF expanded(tl, br);

    if(expanded != sceneRect()) {
        scene_->setSceneRect(expanded);
    }

    QGraphicsView::scrollContentsBy(dx, dy);
}

void GraphView::centerOnPoint(QPointF point)
{
    centerOn(point);
}

void GraphView::reset()
{
    scene_->clear();
    boxes_.clear();
    selected_boxes_.clear();
    profiling_.clear();
    profiling_connections_.clear();

    box_map_.clear();
    proxy_map_.clear();
    port_map_.clear();

    relayed_outputs_widget_ = new PortPanel(graph_facade_, PortPanel::Type::OUTPUT, scene_);
    relayed_outputs_widget_proxy_ = scene_->addWidget(relayed_outputs_widget_);

    relayed_inputs_widget_ = new PortPanel(graph_facade_, PortPanel::Type::INPUT, scene_);
    relayed_inputs_widget_proxy_ = scene_->addWidget(relayed_inputs_widget_);

    update();
}

void GraphView::resetZoom()
{
    resetTransform();
    scene_->setScale(1.0);
}

void GraphView::zoomIn()
{
    zoom(5.0);
}

void GraphView::zoomOut()
{
    zoom(-5.0);
}

void GraphView::zoomAt(QPointF point, double f)
{
    zoom(f);
    centerOn(point);
}

void GraphView::zoom(double f)
{
    qreal factor = 1.0 + f / 25.0;

    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    scale(factor, factor);
    scene_->setScale(transform().m11());
    scene_->invalidateSchema();
}

void GraphView::animateZoom()
{
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    double current_scale = transform().m11();
    if((current_scale < 0.1 && scalings_to_perform_ < 0) ||
            (current_scale > 3.0 && scalings_to_perform_ > 0)) {
        scalings_to_perform_ = 0;
    }

    qreal factor = 1.0 + scalings_to_perform_ / 500.0;
    scale(factor, factor);

    scene_->setScale(transform().m11());
    scene_->invalidateSchema();

    if(scalings_to_perform_ > 0) {
        --scalings_to_perform_;
    } else if(scalings_to_perform_ < 0) {
        ++scalings_to_perform_;
    } else {
        scalings_animation_timer_.stop();
    }
}

DesignerScene* GraphView::designerScene()
{
    return scene_;
}

std::vector<NodeBox*> GraphView::boxes()
{
    return boxes_;
}

std::vector<NodeBox*> GraphView::getSelectedBoxes()
{
    return selected_boxes_;
}

void GraphView::updateSelection()
{    
    selected_boxes_ = scene_->getSelectedBoxes();

    QList<QGraphicsItem *> selected = scene_->items();
    for(QGraphicsItem* item : selected) {
        MovableGraphicsProxyWidget* m = dynamic_cast<MovableGraphicsProxyWidget*>(item);
        if(m) {
            NodeBox* box = m->getBox();
            if(box && box->isVisible()) {
                box->setSelected(m->isSelected());
            }
        }
    }
}

Command::Ptr GraphView::deleteSelected()
{
    command::Meta::Ptr meta(new command::Meta(graph_facade_->getAbsoluteUUID(), "delete selected boxes"));
    for(QGraphicsItem* item : scene_->selectedItems()) {
        MovableGraphicsProxyWidget* proxy = dynamic_cast<MovableGraphicsProxyWidget*>(item);
        if(proxy) {
            meta->add(Command::Ptr(new command::DeleteNode(graph_facade_->getAbsoluteUUID(), proxy->getBox()->getNodeHandle()->getUUID())));
        }
    }
    return meta;
}

void GraphView::keyPressEvent(QKeyEvent* e)
{
    QGraphicsView::keyPressEvent(e);

    if(e->key() == Qt::Key_Space && Qt::ControlModifier != QApplication::keyboardModifiers() && !e->isAutoRepeat()) {
        if(dragMode() != QGraphicsView::ScrollHandDrag) {
            setDragMode(QGraphicsView::ScrollHandDrag);
            setInteractive(false);
            e->accept();
        }
    }
}

void GraphView::keyReleaseEvent(QKeyEvent* e)
{
    QGraphicsView::keyReleaseEvent(e);

    if(e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        setDragMode(QGraphicsView::RubberBandDrag);
        setInteractive(true);
        e->accept();
    }
}

void GraphView::mousePressEvent(QMouseEvent* e)
{
    bool was_rubber_band = false;
    if(e->button() == Qt::MiddleButton) {
        if(dragMode() == QGraphicsView::RubberBandDrag) {
            was_rubber_band = true;
            setDragMode(QGraphicsView::NoDrag);
        }
    }
    QGraphicsView::mousePressEvent(e);

    if(e->button() == Qt::MiddleButton && !e->isAccepted() && !middle_mouse_dragging_) {
        middle_mouse_dragging_ = true;
        middle_mouse_panning_ = false;
        middle_mouse_drag_start_ = e->screenPos();

    } else if(was_rubber_band) {
        setDragMode(QGraphicsView::RubberBandDrag);
    }
}

void GraphView::mouseReleaseEvent(QMouseEvent* e)
{
    QGraphicsView::mouseReleaseEvent(e);

    if(middle_mouse_dragging_) {
        middle_mouse_dragging_ = false;
        middle_mouse_panning_ = false;

        QMouseEvent fake(e->type(), e->pos(), Qt::LeftButton, Qt::LeftButton, e->modifiers());
        QGraphicsView::mouseReleaseEvent(&fake);

        setDragMode(QGraphicsView::RubberBandDrag);
        setInteractive(true);
        e->accept();
    }
}

void GraphView::mouseMoveEvent(QMouseEvent *e)
{
    if(middle_mouse_dragging_ && !middle_mouse_panning_) {
        auto delta = e->screenPos() - middle_mouse_drag_start_;
        if(std::hypot(delta.x(), delta.y()) > 10) {
            middle_mouse_panning_ = true;

            setDragMode(QGraphicsView::ScrollHandDrag);
            setInteractive(false);
            e->accept();

            QMouseEvent fake(e->type(), e->pos(), Qt::LeftButton, Qt::LeftButton, e->modifiers());
            QGraphicsView::mousePressEvent(&fake);
        }
    }

    QGraphicsView::mouseMoveEvent(e);

    setFocus();
}

void GraphView::wheelEvent(QWheelEvent *we)
{
    bool ctrl = Qt::ControlModifier & QApplication::keyboardModifiers();

    if(ctrl) {
        if(scene_->isEmpty()) {
            resetZoom();
            return;
        }

        we->accept();

        int scaleFactor = 4;
        int direction = (we->delta() > 0) ? 1 : -1;

        //        if((direction > 0) != (scalings_to_perform_ > 0)) {
        //            scalings_to_perform_ = 0;
        //        }

        //        scalings_to_perform_ +=  2 * direction * scaleFactor;

        zoom(direction * scaleFactor);

        //        if(!scalings_animation_timer_.isActive()) {
        //            scalings_animation_timer_.setInterval(1000.0 / 60.0);
        //            scalings_animation_timer_.start();
        //        }

    } else {
        QGraphicsView::wheelEvent(we);
    }
}

void GraphView::dragEnterEvent(QDragEnterEvent* e)
{
    delete move_event_;
    move_event_ = nullptr;

    QGraphicsView::dragEnterEvent(e);
    drag_io_.dragEnterEvent(this, e);
}

void GraphView::dragMoveEvent(QDragMoveEvent* e)
{
    delete move_event_;
    move_event_ = new QDragMoveEvent(*e);

    QGraphicsView::dragMoveEvent(e);
    if(!e->isAccepted()) {
        drag_io_.dragMoveEvent(this, e);
    }

    static const int border_threshold = 20;
    static const double scroll_factor = 10.;

    bool scroll_p = false;

    QPointF pos = e->posF();
    if(pos.x() < border_threshold) {
        scroll_p = true;
        scroll_offset_x_ = scroll_factor * (pos.x() - border_threshold) / double(border_threshold);
    } else if(pos.x() > viewport()->width() - border_threshold) {
        scroll_p = true;
        scroll_offset_x_ = scroll_factor * (pos.x() - (viewport()->width() - border_threshold)) / double(border_threshold);
    } else {
        scroll_offset_x_ = 0;
    }

    if(pos.y() < border_threshold) {
        scroll_p = true;
        scroll_offset_y_ = scroll_factor * (pos.y() - border_threshold) / double(border_threshold);
    } else if(pos.y() > viewport()->height() - border_threshold) {
        scroll_p = true;
        scroll_offset_y_ = scroll_factor * (pos.y() - (viewport()->height() - border_threshold)) / double(border_threshold);
    } else {
        scroll_offset_y_ = 0;
    }

    if(scroll_p) {
        if(!scroll_animation_timer_.isActive()) {
            scroll_animation_timer_.start(1000./60.);
        }
    } else {
        if(scroll_animation_timer_.isActive()) {
            scroll_animation_timer_.stop();
        }
    }
}

void GraphView::dropEvent(QDropEvent* e)
{
    delete move_event_;
    move_event_ = nullptr;

    QGraphicsView::dropEvent(e);
    drag_io_.dropEvent(this, e, mapToScene(e->pos()));

    if(scroll_animation_timer_.isActive()) {
        scroll_animation_timer_.stop();
    }
}

void GraphView::dragLeaveEvent(QDragLeaveEvent* e)
{
    delete move_event_;
    move_event_ = nullptr;

    QGraphicsView::dragLeaveEvent(e);

    if(scroll_animation_timer_.isActive()) {
        scroll_animation_timer_.stop();
    }
}

void GraphView::animateScroll()
{
    QScrollBar* h = horizontalScrollBar();
    h->setValue(h->value() + scroll_offset_x_);
    QScrollBar* v = verticalScrollBar();
    v->setValue(v->value() + scroll_offset_y_);

    if(move_event_) {
        drag_io_.dragMoveEvent(this, move_event_);
    }
}

void GraphView::showBoxDialog()
{
    BoxDialog diag(node_factory_);
    int r = diag.exec();

    if(r) {
        std::string type = diag.getName();

        if(!type.empty() && node_factory_.isValidType(type)) {
            Graph* graph = graph_facade_->getGraph();
            UUID uuid = graph->generateUUID(type);
            QPointF pos = mapToScene(mapFromGlobal(QCursor::pos()));
            dispatcher_->executeLater(Command::Ptr(new command::AddNode(graph_facade_->getAbsoluteUUID(), type, Point(pos.x(), pos.y()), uuid, nullptr)));
        }
    }
}


void GraphView::startPlacingBox(const std::string &type, NodeStatePtr state, const QPoint &offset)
{
    NodeConstructorPtr c = node_factory_.getConstructor(type);
    NodeHandlePtr handle = c->makePrototype();

    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData;

    mimeData->setData(NodeBox::MIME, type.c_str());
    if(state) {
        QVariant payload = qVariantFromValue((void *) &state);
        mimeData->setProperty("state", payload);
    }
    mimeData->setProperty("ox", offset.x());
    mimeData->setProperty("oy", offset.y());
    drag->setMimeData(mimeData);

    NodeBox* box = nullptr;

    bool is_note = type == "csapex::Note";

    if(is_note) {
        box = new NoteBox(settings_, handle,
                          QIcon(QString::fromStdString(c->getIcon())));

    } else {
        box = new NodeBox(settings_, handle,
                          QIcon(QString::fromStdString(c->getIcon())));
    }
    box->setAdapter(std::make_shared<DefaultNodeAdapter>(handle, box));

    if(state) {
        handle->setNodeState(state);
    }
    box->setStyleSheet(styleSheet());
    box->construct();
    box->setObjectName(handle->getType().c_str());

    if(!is_note) {
        box->setLabel(type);
    }

    drag->setPixmap(box->grab());
    drag->setHotSpot(-offset);
    drag->exec();

    delete box;
}


void GraphView::nodeAdded(NodeWorkerPtr node_worker)
{
    NodeHandlePtr node_handle = node_worker->getNodeHandle();
    std::string type = node_handle->getType();

    QIcon icon = QIcon(QString::fromStdString(node_factory_.getConstructor(type)->getIcon()));

    NodeBox* box = nullptr;
    if(type == "csapex::Note") {
        box = new NoteBox(settings_, node_handle, node_worker, icon, this);
    } else {
        box = new NodeBox(settings_, node_handle, node_worker, icon, this);
    }

    QObject::connect(box, SIGNAL(portAdded(Port*)), this, SLOT(addPort(Port*)));
    QObject::connect(box, SIGNAL(portRemoved(Port*)), this, SLOT(removePort(Port*)));

    NodeAdapter::Ptr adapter = node_adapter_factory_.makeNodeAdapter(node_handle, box);
    adapter->executeCommand.connect(delegate::Delegate<void(CommandPtr)>(dispatcher_, &CommandDispatcher::execute));
    box->setAdapter(adapter);

    box_map_[node_handle->getUUID()] = box;
    proxy_map_[node_handle->getUUID()] = new MovableGraphicsProxyWidget(box, this, parent_->options());

    box->construct();

    addBox(box);

    // add existing connectors
    for(auto input : node_handle->getAllInputs()) {
        connectorMessageAdded(input);
    }
    for(auto output : node_handle->getAllOutputs()) {
        connectorMessageAdded(output);
    }
    for(auto slot : node_handle->getAllSlots()) {
        connectorSignalAdded(slot);
    }
    for(auto trigger : node_handle->getAllTriggers()) {
        connectorSignalAdded(trigger);
    }

    // subscribe to coming connectors
    auto c1 = node_handle->connectorCreated.connect([this](ConnectablePtr c) { triggerConnectorCreated(c); });
    handle_connections_[node_handle.get()].emplace_back(c1);
    auto c2 = node_handle->connectorRemoved.connect([this](ConnectablePtr c) { triggerConnectorRemoved(c); });
    handle_connections_[node_handle.get()].emplace_back(c2);


    UUID uuid = node_handle->getUUID();
    QObject::connect(box, &NodeBox::toggled, [this, uuid](bool checked) {
        dispatcher_->execute(std::make_shared<command::DisableNode>(graph_facade_->getAbsoluteUUID(), uuid, !checked));
    });

    Q_EMIT boxAdded(box);
}


void GraphView::connectorCreated(ConnectablePtr connector)
{
    // TODO: dirty...
    if(dynamic_cast<Slot*> (connector.get()) || dynamic_cast<Trigger*>(connector.get())) {
        connectorSignalAdded(connector);
    } else {
        connectorMessageAdded(connector);
    }
}

void GraphView::connectorRemoved(ConnectablePtr connector)
{
}


void GraphView::connectorMessageAdded(ConnectablePtr connector)
{
    UUID parent_uuid = connector->getUUID().parentUUID();

    Graph* g = graph_facade_->getGraph();
    NodeHandle* node_worker = g->findNodeHandle(parent_uuid);
    if(node_worker) {
        Output* o = dynamic_cast<Output*>(connector.get());
        if(o && node_worker->isParameterOutput(o)) {
            return;
        }

        Input* i = dynamic_cast<Input*>(connector.get());
        if(i && node_worker->isParameterInput(i)) {
            return;
        }

        NodeBox* box = getBox(parent_uuid);
        QBoxLayout* layout = connector->isInput() ? box->getInputLayout() : box->getOutputLayout();

        box->createPort(connector, layout);
    }
}

void GraphView::connectorSignalAdded(ConnectablePtr connector)
{
    UUID parent_uuid = connector->getUUID().parentUUID();
    NodeBox* box = getBox(parent_uuid);
    QBoxLayout* layout = dynamic_cast<Trigger*>(connector.get()) ? box->getTriggerLayout() : box->getSlotLayout();

    box->createPort(connector, layout);
}

NodeBox* GraphView::getBox(const UUID &node_id)
{
    auto pos = box_map_.find(node_id);
    if(pos == box_map_.end()) {
        return nullptr;
    }

    return pos->second;
}

MovableGraphicsProxyWidget* GraphView::getProxy(const UUID &node_id)
{
    auto pos = proxy_map_.find(node_id);
    if(pos == proxy_map_.end()) {
        return nullptr;
    }

    return pos->second;
}

GraphFacade* GraphView::getGraphFacade() const
{
    return graph_facade_.get();
}

void GraphView::nodeRemoved(NodeHandlePtr node_handle)
{
    UUID node_uuid = node_handle->getUUID();
    NodeBox* box = getBox(node_uuid);
    box->stop();

    box_map_.erase(box_map_.find(node_uuid));

    removeBox(box);

    box->deleteLater();

    Q_EMIT boxRemoved(box);
}

void GraphView::addBox(NodeBox *box)
{
    Graph* graph = graph_facade_->getGraph();

    QObject::connect(box, SIGNAL(renameRequest(NodeBox*)), this, SLOT(renameBox(NodeBox*)));

    NodeWorker* worker = box->getNodeWorker();
    NodeHandle* handle = worker->getNodeHandle().get();

    worker_connections_[worker].emplace_back(handle->connectionStart.connect([this](Connectable*) { scene_->deleteTemporaryConnections(); }));
    worker_connections_[worker].emplace_back(handle->connectionInProgress.connect([this](Connectable* from, Connectable* to) { scene_->previewConnection(from, to); }));
    worker_connections_[worker].emplace_back(handle->connectionDone.connect([this](Connectable*) { scene_->deleteTemporaryConnectionsAndRepaint(); }));

    worker_connections_[worker].emplace_back(graph->structureChanged.connect([this](Graph*){ updateBoxInformation(); }));

    QObject::connect(box, SIGNAL(showContextMenuForBox(NodeBox*,QPoint)), this, SLOT(showContextMenuForSelectedNodes(NodeBox*,QPoint)));

    worker_connections_[worker].emplace_back(worker->startProfiling.connect([this](NodeWorker* nw) { startProfilingRequest(nw); }));
    worker_connections_[worker].emplace_back(worker->stopProfiling.connect([this](NodeWorker* nw) { stopProfilingRequest(nw); }));


    MovableGraphicsProxyWidget* proxy = getProxy(box->getNodeWorker()->getUUID());
    scene_->addItem(proxy);

    QObject::connect(proxy, SIGNAL(moved(double,double)), this, SLOT(movedBoxes(double,double)));

    boxes_.push_back(box);

    box->setStyleSheet(styleSheet());

    for(QGraphicsItem *item : items()) {
        item->setFlag(QGraphicsItem::ItemIsMovable);
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
        item->setScale(1.0);
    }

    box->init();
    box->triggerPlaced();

    box->updateBoxInformation(graph);
}

void GraphView::removeBox(NodeBox *box)
{
    handle_connections_.erase(box->getNodeHandle());
    worker_connections_.erase(box->getNodeWorker());

    box->setVisible(false);
    box->deleteLater();

    boxes_.erase(std::find(boxes_.begin(), boxes_.end(), box));
    profiling_.erase(box);
}

void GraphView::addPort(Port *port)
{
    scene_->addPort(port);

    QObject::connect(port, SIGNAL(mouseOver(Port*)), this, SLOT(showPreview(Port*)));
    QObject::connect(port, SIGNAL(mouseOut(Port*)), this, SLOT(stopPreview()));

    QObject::connect(port, &Port::removeConnectionsRequest, [this, port]() {
        ConnectablePtr adaptee = port->getAdaptee().lock();
        if(!adaptee) {
            return;
        }
        dispatcher_->execute(CommandFactory(graph_facade_.get()).removeAllConnectionsCmd(adaptee));
    });

    QObject::connect(port, &Port::addConnectionRequest, [this, port](Connectable* from) {
        ConnectablePtr adaptee = port->getAdaptee().lock();
        if(!adaptee) {
            return;
        }
        auto cmd = CommandFactory(graph_facade_.get()).addConnection(adaptee->getUUID(), from->getUUID());
        dispatcher_->execute(cmd);
    });

    QObject::connect(port, &Port::moveConnectionRequest, [this, port](Connectable* from) {
        ConnectablePtr adaptee = port->getAdaptee().lock();
        if(!adaptee) {
            return;
        }
        if(!from->isVirtual() && !adaptee->isVirtual()) {
            Command::Ptr cmd(new command::MoveConnection(graph_facade_->getAbsoluteUUID(), from, adaptee.get()));
             dispatcher_->execute(cmd);
        }
    });
}

void GraphView::removePort(Port *port)
{
    scene_->removePort(port);
}

void GraphView::renameBox(NodeBox *box)
{
    bool ok;
    QString text = QInputDialog::getText(this, "Box Label", "Enter new name", QLineEdit::Normal, box->getLabel().c_str(), &ok);

    if(ok && !text.isEmpty()) {
        box->setLabel(text);
    }
}


void GraphView::startProfiling(NodeWorker *node)
{
    NodeBox* box = getBox(node->getUUID());
    apex_assert_hard(profiling_.find(box) == profiling_.end());

    ProfilingWidget* prof = new ProfilingWidget(this, box);
    profiling_[box] = prof;

    QGraphicsProxyWidget* prof_proxy = scene_->addWidget(prof);
    prof->reposition(prof_proxy->pos().x(), prof_proxy->pos().y());
    prof->show();


    foreach (QGraphicsItem *item, items()) {
        item->setFlag(QGraphicsItem::ItemIsMovable);
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
        item->setScale(1.0);
    }

    MovableGraphicsProxyWidget* proxy = getProxy(box->getNodeWorker()->getUUID());
    QObject::connect(proxy, SIGNAL(moving(double,double)), prof, SLOT(reposition(double,double)));

    auto nw = box->getNodeWorker();

    auto cp = nw->messages_processed.connect([prof](){ prof->update(); });
    profiling_connections_[box].push_back(cp);

    auto ct = nw->ticked.connect([prof](){ prof->update(); });
    profiling_connections_[box].push_back(ct);
}

void GraphView::stopProfiling(NodeWorker *node)
{
    NodeBox* box = getBox(node->getUUID());

    for(auto& connection : profiling_connections_[box]) {
        connection.disconnect();
    }
    profiling_connections_.erase(box);

    std::map<NodeBox*, ProfilingWidget*>::iterator pos = profiling_.find(box);
    apex_assert_hard(pos != profiling_.end());

    pos->second->deleteLater();
    //    delete pos->second;
    profiling_.erase(pos);
}

void GraphView::movedBoxes(double dx, double dy)
{
    QPointF delta(dx, dy);
    command::Meta::Ptr meta(new command::Meta(graph_facade_->getAbsoluteUUID(), "move boxes"));
    for(QGraphicsItem* item : scene_->selectedItems()) {
        MovableGraphicsProxyWidget* proxy = dynamic_cast<MovableGraphicsProxyWidget*>(item);
        if(proxy) {
            NodeBox* b = proxy->getBox();
            QPointF to = proxy->pos();
            QPointF from = to - delta;
            meta->add(Command::Ptr(new command::MoveBox(graph_facade_->getAbsoluteUUID(),
                                                        b->getNodeWorker()->getUUID(),
                                                        Point(from.x(), from.y()), Point(to.x(), to.y()),
                                                        parent_)));
        }
    }
    dispatcher_->execute(meta);

    scene_->invalidateSchema();
}

void GraphView::overwriteStyleSheet(const QString &stylesheet)
{
    setStyleSheet(stylesheet);

    scene_->update();

    for (NodeBox *box : boxes_) {
        box->setStyleSheet(stylesheet);
        box->updateVisuals();
    }

    relayed_outputs_widget_->setStyleSheet(stylesheet);
    relayed_inputs_widget_->setStyleSheet(stylesheet);
}

void GraphView::updateBoxInformation()
{
    for(QGraphicsItem* item : scene_->items()) {
        MovableGraphicsProxyWidget* proxy = dynamic_cast<MovableGraphicsProxyWidget*>(item);
        if(proxy) {
            NodeBox* b = proxy->getBox();
            b->updateBoxInformation(graph_facade_->getGraph());
        }
    }
}

void GraphView::showContextMenuGlobal(const QPoint& global_pos)
{
    QMenu menu;

    QAction q_copy("copy", &menu);
    q_copy.setIcon(QIcon(":/copy.png"));
    q_copy.setEnabled(!getSelectedBoxes().empty());
    menu.addAction(&q_copy);

    QAction q_paste("paste", &menu);
    q_paste.setIcon(QIcon(":/paste.png"));
    q_paste.setEnabled(ClipBoard::canPaste());
    menu.addAction(&q_paste);

    menu.addSeparator();

    QAction add_note("create sticky note", &menu);
    add_note.setIcon(QIcon(":/note.png"));
    menu.addAction(&add_note);

    QMenu add_node("create node");
    add_node.setIcon(QIcon(":/plugin.png"));
    NodeListGenerator generator(node_factory_);
    generator.insertAvailableNodeTypes(&add_node);
    menu.addMenu(&add_node);

    QAction* selectedItem = menu.exec(global_pos);

    if(selectedItem) {
        if(selectedItem == &q_copy) {
            copySelected();

        } else if(selectedItem == &q_paste) {
            paste();

        } else if(selectedItem == &add_note) {
            startPlacingBox("csapex::Note", nullptr);

        } else {
            // else it must be an insertion
            std::string selected = selectedItem->data().toString().toStdString();
            startPlacingBox(selected, nullptr);
        }
    }
}

void GraphView::showContextMenuForSelectedNodes(NodeBox* box, const QPoint &scene_pos)
{
    if(std::find(selected_boxes_.begin(), selected_boxes_.end(), box) == selected_boxes_.end()) {
        scene_->setSelection(box);
        updateSelection();

    } else if(selected_boxes_.empty()) {
        selected_boxes_.push_back(box);
    }

    std::stringstream title;
    if(selected_boxes_.size() == 1) {
        title << "Node: " << selected_boxes_.front()->getNodeWorker()->getUUID().getShortName();
    } else {
        title << selected_boxes_.size() << " selected nodes";
    }

    QMenu menu;
    std::map<QAction*, std::function<void()> > handler;

    ContextMenuHandler::addHeader(menu, title.str());


    QAction* copy = new QAction("copy", &menu);
    copy->setIcon(QIcon(":/copy.png"));
    copy->setIconVisibleInMenu(true);
    handler[copy] = std::bind(&GraphView::copySelected, this);
    menu.addAction(copy);

    QAction* paste = new QAction("paste", &menu);
    paste->setIcon(QIcon(":/paste.png"));
    paste->setEnabled(false);
    menu.addAction(paste);

    menu.addSeparator();

    bool has_minimized = false;
    bool has_maximized = false;
    bool has_box = false;
    bool has_note = false;
    for(NodeBox* box : selected_boxes_) {
        bool m = box->isMinimizedSize();
        has_minimized |= m;
        has_maximized |= !m;

        bool is_note = dynamic_cast<NoteBox*>(box);
        has_note |= is_note;
        has_box |= !is_note;
    }


    if(has_box) {
        if(has_minimized) {
            QAction* max = new QAction("maximize", &menu);
            max->setIcon(QIcon(":/maximize.png"));
            max->setIconVisibleInMenu(true);
            handler[max] = std::bind(&GraphView::minimizeBox, this, false);
            menu.addAction(max);
        }
        if(has_maximized){
            QAction* min = new QAction("minimize", &menu);
            min->setIcon(QIcon(":/minimize.png"));
            min->setIconVisibleInMenu(true);
            handler[min] = std::bind(&GraphView::minimizeBox, this, true);
            menu.addAction(min);
        }

        QAction* flip = new QAction("flip sides", &menu);
        flip->setIcon(QIcon(":/flip.png"));
        flip->setIconVisibleInMenu(true);
        handler[flip] = std::bind(&GraphView::flipBox, this);
        menu.addAction(flip);

        menu.addSeparator();

        bool threading = !settings_.get("threadless", false);
        QMenu* thread_menu = menu.addMenu(QIcon(":/thread_group.png"), "thread grouping");
        thread_menu->setEnabled(threading);

        if(thread_menu->isEnabled()) {
            QAction* private_thread = new QAction("private thread", &menu);
            private_thread->setIcon(QIcon(":/thread_group_none.png"));
            private_thread->setIconVisibleInMenu(true);
            handler[private_thread] = std::bind(&GraphView::usePrivateThreadFor, this);
            thread_menu->addAction(private_thread);

            QAction* default_thread = new QAction("default thread", &menu);
            default_thread->setIcon(QIcon(":/thread_group.png"));
            default_thread->setIconVisibleInMenu(true);
            handler[default_thread] = std::bind(&GraphView::useDefaultThreadFor, this);
            thread_menu->addAction(default_thread);

            thread_menu->addSeparator();

            QMenu* choose_group_menu = new QMenu("thread group", &menu);


            ThreadPool* thread_pool = graph_facade_->getThreadPool();

            std::vector<ThreadGroupPtr> thread_groups = thread_pool->getGroups();
            for(std::size_t i = 0; i < thread_groups.size(); ++i) {
                const ThreadGroup& group = *thread_groups[i];

                if(group.id() == ThreadGroup::PRIVATE_THREAD) {
                    continue;
                }

                std::stringstream ss;
                ss << "(" << group.id() << ") " << group.name();
                QAction* switch_thread = new QAction(QString::fromStdString(ss.str()), &menu);
                switch_thread->setIcon(QIcon(":/thread_group.png"));
                switch_thread->setIconVisibleInMenu(true);
                handler[switch_thread] = std::bind(&GraphView::switchToThread, this, group.id());
                choose_group_menu->addAction(switch_thread);
            }

            choose_group_menu->addSeparator();

            QAction* new_group = new QAction("new thread group", &menu);
            new_group->setIcon(QIcon(":/thread_group_add.png"));
            new_group->setIconVisibleInMenu(true);
            //        handler[new_group] = std::bind(&ThreadPool::createNewThreadGroupFor, &thread_pool_,  box->getNodeWorker());
            handler[new_group] = std::bind(&GraphView::createNewThreadGroupFor, this);

            choose_group_menu->addAction(new_group);

            thread_menu->addMenu(choose_group_menu);
        }

        //    QAction* term = new QAction("terminate thread", &menu);
        //    term->setIcon(QIcon(":/stop.png"));
        //    term->setIconVisibleInMenu(true);
        //    handler[term] = std::bind(&NodeBox::killContent, box);
        //    menu.addAction(term);

        menu.addSeparator();


        bool has_profiling = false;
        bool has_not_profiling = false;
        for(NodeBox* box : selected_boxes_) {
            bool p = box->getNodeWorker()->isProfiling();
            has_profiling |= p;
            has_not_profiling |= !p;
        }
        if(has_profiling) {
            QAction* prof = new QAction("stop profiling", &menu);
            prof->setIcon(QIcon(":/stop_profiling.png"));
            prof->setIconVisibleInMenu(true);
            handler[prof] = std::bind(&GraphView::showProfiling, this, false);
            menu.addAction(prof);
        }
        if(has_not_profiling){
            QAction* prof = new QAction("start profiling", &menu);
            prof->setIcon(QIcon(":/profiling.png"));
            prof->setIconVisibleInMenu(true);
            handler[prof] = std::bind(&GraphView::showProfiling, this, true);
            menu.addAction(prof);
        }

        if(selected_boxes_.size() == 1) {
            QAction* info = new QAction("get information", &menu);
            info->setIcon(QIcon(":/help.png"));
            info->setIconVisibleInMenu(true);
            handler[info] = std::bind(&NodeBox::getInformation, selected_boxes_.front());
            menu.addAction(info);
        }

        menu.addSeparator();

    }

    QAction* set_color = new QAction("set color", &menu);
    set_color->setIcon(QIcon(":/color_wheel.png"));
    set_color->setIconVisibleInMenu(true);
    handler[set_color] = std::bind(&GraphView::chooseColor, this);
    menu.addAction(set_color);

    QAction* grp = new QAction("group into subgraph", &menu);
    grp->setIcon(QIcon(":/group.png"));
    grp->setIconVisibleInMenu(true);
    handler[grp] = std::bind(&GraphView::groupBox, this);
    menu.addAction(grp);

    menu.addSeparator();

    QAction* del = new QAction("delete", &menu);
    del->setIcon(QIcon(":/close.png"));
    del->setIconVisibleInMenu(true);
    handler[del] = std::bind(&GraphView::deleteBox, this);
    menu.addAction(del);

    QAction* selectedItem = menu.exec(mapToGlobal(mapFromScene(scene_pos)));

    if(selectedItem) {
        handler[selectedItem]();
    }
}


void GraphView::usePrivateThreadFor()
{
    command::Meta::Ptr cmd(new command::Meta(graph_facade_->getAbsoluteUUID(),"use private thread"));
    for(NodeBox* box : selected_boxes_) {
        cmd->add(Command::Ptr(new command::SwitchThread(graph_facade_->getAbsoluteUUID(),box->getNodeWorker()->getUUID(), ThreadGroup::PRIVATE_THREAD)));
    }
    dispatcher_->execute(cmd);
}


void GraphView::useDefaultThreadFor()
{
    command::Meta::Ptr cmd(new command::Meta(graph_facade_->getAbsoluteUUID(),"use private thread"));
    for(NodeBox* box : selected_boxes_) {
        cmd->add(Command::Ptr(new command::SwitchThread(graph_facade_->getAbsoluteUUID(),box->getNodeWorker()->getUUID(), ThreadGroup::DEFAULT_GROUP_ID)));
    }
    dispatcher_->execute(cmd);
}

void GraphView::switchToThread(int group_id)
{
    command::Meta::Ptr cmd(new command::Meta(graph_facade_->getAbsoluteUUID(),"switch thread"));
    for(NodeBox* box : selected_boxes_) {
        cmd->add(Command::Ptr(new command::SwitchThread(graph_facade_->getAbsoluteUUID(),box->getNodeWorker()->getUUID(), group_id)));
    }
    dispatcher_->execute(cmd);
}

void GraphView::createNewThreadGroupFor()
{
    bool ok;
    ThreadPool* thread_pool = graph_facade_->getThreadPool();
    QString text = QInputDialog::getText(this, "Group Name", "Enter new name", QLineEdit::Normal, QString::fromStdString(thread_pool->nextName()), &ok);

    if(ok && !text.isEmpty()) {
        command::Meta::Ptr cmd(new command::Meta(graph_facade_->getAbsoluteUUID(),"create new thread group"));
        for(NodeBox* box : selected_boxes_) {
            cmd->add(Command::Ptr(new command::CreateThread(graph_facade_->getAbsoluteUUID(),box->getNodeWorker()->getUUID(), text.toStdString())));
        }
        dispatcher_->execute(cmd);
    }
}

void GraphView::showProfiling(bool show)
{
    for(NodeBox* box : selected_boxes_) {
        box->showProfiling(show);
    }
}

void GraphView::chooseColor()
{
    QColor c = QColorDialog::getColor();

    if(!c.isValid()) {
        return;
    }

    int r = c.red();
    int g = c.green();
    int b = c.blue();

    command::Meta::Ptr cmd(new command::Meta(graph_facade_->getAbsoluteUUID(),"flip boxes"));
    for(NodeBox* box : selected_boxes_) {
        cmd->add(Command::Ptr(new command::SetColor(graph_facade_->getAbsoluteUUID(),box->getNodeWorker()->getUUID(), r, g, b)));
    }
    dispatcher_->execute(cmd);
}

void GraphView::flipBox()
{
    command::Meta::Ptr cmd(new command::Meta(graph_facade_->getAbsoluteUUID(),"flip boxes"));
    for(NodeBox* box : selected_boxes_) {
        cmd->add(Command::Ptr(new command::FlipSides(graph_facade_->getAbsoluteUUID(),box->getNodeWorker()->getUUID())));
    }
    dispatcher_->execute(cmd);
}

void GraphView::minimizeBox(bool mini)
{
    command::Meta::Ptr cmd(new command::Meta(graph_facade_->getAbsoluteUUID(),(mini ? std::string("minimize") : std::string("maximize")) + " boxes"));
    for(NodeBox* box : selected_boxes_) {
        cmd->add(Command::Ptr(new command::Minimize(graph_facade_->getAbsoluteUUID(),box->getNodeWorker()->getUUID(), mini)));
    }
    dispatcher_->execute(cmd);
}

void GraphView::deleteBox()
{
    command::Meta::Ptr cmd(new command::Meta(graph_facade_->getAbsoluteUUID(),"delete boxes"));
    for(NodeBox* box : selected_boxes_) {
        cmd->add(Command::Ptr(new command::DeleteNode(graph_facade_->getAbsoluteUUID(),box->getNodeWorker()->getUUID())));
    }
    dispatcher_->execute(cmd);
}

void GraphView::groupBox()
{
    std::vector<UUID> uuids;
    uuids.reserve(selected_boxes_.size());
    for(NodeBox* box : selected_boxes_) {
        uuids.push_back(box->getNodeHandle()->getUUID());
    }
    CommandPtr cmd(new command::GroupNodes(graph_facade_->getAbsoluteUUID(),uuids));
    dispatcher_->execute(cmd);
}


void GraphView::copySelected()
{
    GraphIO io(graph_facade_->getGraph(), &node_factory_);

    std::vector<UUID> nodes;
    for(const NodeBox* box : getSelectedBoxes()) {
        nodes.emplace_back(box->getNodeHandle()->getUUID());
    }

    YAML::Node yaml;
    io.saveSelectedGraph(yaml, nodes);

    ClipBoard::set(yaml);
}

void GraphView::paste()
{
    apex_assert_hard(ClipBoard::canPaste());
    YAML::Node blueprint = YAML::Load(ClipBoard::get());

    QPointF pos = mapToScene(mapFromGlobal(QCursor::pos()));

    AUUID graph_id = graph_facade_->getAbsoluteUUID();
    CommandPtr cmd(new command::PasteGraph(graph_id, blueprint, Point (pos.x(), pos.y())));

    dispatcher_->execute(cmd);
}

void GraphView::contextMenuEvent(QContextMenuEvent* event)
{
    if(scene_->getHighlightedConnectionId() != -1) {
        scene_->showConnectionContextMenu();
        return;
    }

    QGraphicsView::contextMenuEvent(event);

    if(!event->isAccepted()) {
        showContextMenuGlobal(event->globalPos());
    }
}

void GraphView::selectAll()
{
    for(QGraphicsItem* item : scene_->items()) {
        item->setSelected(true);
    }
}

void GraphView::showPreview(Port* port)
{
    QPointF pos = mapToScene(mapFromGlobal(QCursor::pos()));
    pos.setY(pos.y() + 50);

    if(!preview_widget_) {
        preview_widget_ = new MessagePreviewWidget;
        preview_widget_->hide();
    }

    preview_widget_->setWindowTitle(QString::fromStdString("Output"));
    preview_widget_->move(pos.toPoint());

    if(!preview_widget_->isConnected()) {
        preview_widget_->connectTo(port->getAdaptee().lock().get());
        designerScene()->addWidget(preview_widget_);
    }
}

void GraphView::stopPreview()
{
    if(preview_widget_) {
        preview_widget_->disconnect();
        preview_widget_->hide();
        preview_widget_->deleteLater();
        preview_widget_ = nullptr;
    }
}

/// MOC
#include "../../../include/csapex/view/designer/moc_graph_view.cpp"
