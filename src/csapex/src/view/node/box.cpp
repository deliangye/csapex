/// HEADER
#include <csapex/view/node/box.h>

/// COMPONENT
#include "ui_box.h"
#include <csapex/model/node.h>
#include <csapex/factory/node_factory.h>
#include <csapex/msg/input.h>
#include <csapex/msg/output.h>
#include <csapex/model/node_handle.h>
#include <csapex/model/node_worker.h>
#include <csapex/model/node_state.h>
#include <csapex/msg/input_transition.h>
#include <csapex/msg/output_transition.h>
#include <csapex/command/meta.h>
#include <csapex/command/dispatcher.h>
#include <csapex/view/node/node_adapter.h>
#include <csapex/view/utility/color.hpp>
#include <csapex/core/settings.h>
#include <csapex/view/widgets/port.h>
#include <csapex/view/designer/graph_view.h>
#include <csapex/model/graph_facade.h>

/// SYSTEM
#include <QDragMoveEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QMenu>
#include <QTimer>
#include <QPainter>
#include <iostream>
#include <QSizeGrip>
#include <QThread>
#include <cmath>

using namespace csapex;

const QString NodeBox::MIME = "csapex/model/box";

NodeBox::NodeBox(Settings& settings, NodeHandlePtr handle, NodeWorker::Ptr worker, QIcon icon, GraphView* parent)
    : parent_(parent), ui(new Ui::Box), grip_(nullptr), settings_(settings), node_handle_(handle), node_worker_(worker), adapter_(nullptr), icon_(icon),
      down_(false), info_exec(nullptr), info_compo(nullptr), info_thread(nullptr), info_error(nullptr), initialized_(false)
{
    handle->getNodeState()->flipped_changed->connect(std::bind(&NodeBox::triggerFlipSides, this));
    handle->getNodeState()->minimized_changed->connect(std::bind(&NodeBox::triggerMinimized, this));

    QObject::connect(this, SIGNAL(updateVisualsRequest()), this, SLOT(updateVisuals()));

    setVisible(false);
}

NodeBox::NodeBox(Settings& settings, NodeHandlePtr handle, QIcon icon, GraphView* parent)
    : NodeBox(settings, handle, nullptr, icon, parent)
{
}

void NodeBox::setAdapter(NodeAdapter::Ptr adapter)
{
    adapter_ = adapter;

    if(adapter->isResizable()) {
        grip_ = new QSizeGrip(this);
        grip_->installEventFilter(this);
    }
}

NodeBox::~NodeBox()
{
    delete ui;
}


void NodeBox::setupUi()
{
    if(!info_exec) {
        info_exec = new QLabel;
        info_exec->setProperty("exec", true);
        ui->infos->addWidget(info_exec);
    }
    if(!info_compo) {
        info_compo = new QLabel;
        info_compo->setProperty("component", true);
        ui->infos->addWidget(info_compo);
    }

    if(!info_thread) {
        info_thread = new QLabel;
        info_thread->setProperty("threadgroup", true);
        ui->infos->addWidget(info_thread);
    }

    if(!info_error) {
        info_error = new QLabel;
        info_error->setText("<img src=':/error.png' />");
        info_error->setProperty("error", true);
        info_error->setVisible(false);
        ui->infos->addWidget(info_error);
    }


    setupUiAgain();

    Q_EMIT changed(this);
}

void NodeBox::setupUiAgain()
{
    ui->header->setAlignment(Qt::AlignTop);
    ui->content->setAlignment(Qt::AlignTop);

    if(adapter_) {
        adapter_->doSetupUi(ui->content);
    }
    setAutoFillBackground(false);

    setAttribute( Qt::WA_TranslucentBackground, true );
    setAttribute(Qt::WA_NoSystemBackground, true);
    //    setBackgroundMode (Qt::NoBackground, true);

    updateVisuals();
}

void NodeBox::construct()
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }

    ui->setupUi(this);

    ui->input_layout->addSpacerItem(new QSpacerItem(16, 0));
    ui->output_layout->addSpacerItem(new QSpacerItem(16, 0));

    ui->enablebtn->setCheckable(true);
    ui->enablebtn->setChecked(nh->getNodeState()->isEnabled());

    QSize size(16, 16);
    ui->icon->setPixmap(icon_.pixmap(size));

    setFocusPolicy(Qt::ClickFocus);

    const UUID& uuid = nh->getUUID();
    setToolTip(QString::fromStdString(uuid.getFullName()));

    setObjectName(QString::fromStdString(uuid.getFullName()));

    installEventFilter(this);

    ui->content->installEventFilter(this);
    ui->label->installEventFilter(this);

    setLabel(nh->getNodeState()->getLabel());

    QObject::connect(ui->enablebtn, SIGNAL(toggled(bool)), this, SIGNAL(toggled(bool)));

    nh->nodeStateChanged.connect([this]() { nodeStateChanged(); });
    QObject::connect(this, SIGNAL(nodeStateChanged()), this, SLOT(nodeStateChangedEvent()), Qt::QueuedConnection);

    nh->connectorCreated.connect([this](ConnectablePtr c) { registerEvent(c.get()); });
    nh->connectorRemoved.connect([this](ConnectablePtr c) { unregisterEvent(c.get()); });

    NodeWorkerPtr worker = node_worker_.lock();
    if(worker) {
        enabledChangeEvent(worker->isProcessingEnabled());
        //    nh->enabled.connect([this](bool e){ enabledChange(e); });
        QObject::connect(this, SIGNAL(enabledChange(bool)), this, SLOT(enabledChangeEvent(bool)), Qt::QueuedConnection);

        worker->threadChanged.connect([this](){ updateThreadInformation(); });
        worker->errorHappened.connect([this](bool){ updateVisualsRequest(); });
    }


    for(auto input : nh->getAllInputs()) {
        registerInputEvent(input.get());
    }
    for(auto output : nh->getAllOutputs()) {
        registerOutputEvent(output.get());
    }

    setupUi();
}

Node* NodeBox::getNode() const
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return nullptr;
    }
    return nh->getNode().lock().get();
}

NodeWorker* NodeBox::getNodeWorker() const
{
    NodeWorkerPtr worker = node_worker_.lock();
    if(!worker) {
        return nullptr;
    }
    return worker.get();
}

NodeHandle* NodeBox::getNodeHandle() const
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return nullptr;
    }
    return nh.get();
}

NodeAdapter::Ptr NodeBox::getNodeAdapter() const
{
    return adapter_;
}

GraphView* NodeBox::getGraphView() const
{
    return parent_;
}


void NodeBox::updateBoxInformation(Graph* graph)
{
    updateComponentInformation(graph);
    updateThreadInformation();
}

namespace {
void setStyleForId(QLabel* label, int id) {
    // set color using HSV rotation
    double hue =  (id * 77) % 360;
    double r = 0, g = 0, b = 0;
    __HSV2RGB__(hue, 1., 1., r, g, b);
    double fr = 0, fb = 0, fg = 0;
    double min = std::min(b, std::min(g, r));
    double max = std::max(b, std::max(g, r));
    if(min < 100 && max < 100) {
        fr = fb = fg = 255;
    }
    std::stringstream ss;
    ss << "QLabel { background-color : rgb(" << r << "," << g << "," << b << "); color: rgb(" << fr << "," << fg << "," << fb << ");}";
    label->setStyleSheet(ss.str().c_str());
}
}

void NodeBox::updateComponentInformation(Graph* graph)
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }

    if(!settings_.get<bool>("display-graph-components", false)) {
        info_compo->setVisible(false);
        return;
    } else {
        info_compo->setVisible(true);
    }

    if(info_compo) {
        int compo = graph->getComponent(nh->getUUID());
        int level = graph->getLevel(nh->getUUID());
        std::stringstream info;
        info << "C:" << compo;
        info << "L:" << level;
        info_compo->setText(info.str().c_str());

        setStyleForId(info_compo, compo);
    }
}

void NodeBox::updateThreadInformation()
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }

    if(!settings_.get<bool>("display-threads", false)) {
        info_thread->setVisible(false);
        return;
    } else {
        info_thread->setVisible(true);
    }

    if(info_thread) {
        NodeStatePtr state = nh->getNodeState();
        int id = state->getThreadId();
        std::stringstream info;
        if(settings_.get<bool>("threadless")) {
            info << "<i><b><small>threadless</small></b></i>";
            info_thread->setStyleSheet("QLabel { background-color : rgb(0,0,0); color: rgb(255,255,255);}");
        } else if(id < 0) {
            info << "T:" << -id;
            setStyleForId(info_thread, id);
        } else if(id == 0) {
            info << "<i><b><small>private</small></b></i>";
            info_thread->setStyleSheet("QLabel { background-color : rgb(255,255,255); color: rgb(0,0,0);}");
        } else {
            info << state->getThreadName();
            setStyleForId(info_thread, id);
        }
        info_thread->setText(info.str().c_str());
    }
}

void NodeBox::contextMenuEvent(QContextMenuEvent* e)
{
    Q_EMIT showContextMenuForBox(this, e->globalPos());
}

QBoxLayout* NodeBox::getInputLayout()
{
    return ui->input_layout;
}

QBoxLayout* NodeBox::getOutputLayout()
{
    return ui->output_layout;
}

QBoxLayout* NodeBox::getSlotLayout()
{
    return ui->slot_layout;
}

QBoxLayout* NodeBox::getTriggerLayout()
{
    return ui->signal_layout;
}

bool NodeBox::isError() const
{
    NodeWorkerPtr worker = node_worker_.lock();
    if(!worker) {
        return false;
    }
    return worker->isError();
}
ErrorState::ErrorLevel NodeBox::errorLevel() const
{
    NodeWorkerPtr worker = node_worker_.lock();
    if(!worker) {
        return ErrorState::ErrorLevel::NONE;
    }
    return worker->errorLevel();
}
std::string NodeBox::errorMessage() const
{
    NodeWorkerPtr worker = node_worker_.lock();
    if(!worker) {
        return "";
    }
    return worker->errorMessage();
}

void NodeBox::setLabel(const std::string& label)
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }
    NodeStatePtr state = nh->getNodeState();

    apex_assert_hard(state);
    state->setLabel(label);
    ui->label->setText(label.c_str());
    ui->label->setToolTip(label.c_str());
}

void NodeBox::setLabel(const QString &label)
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }
    NodeStatePtr state = nh->getNodeState();
    state->setLabel(label.toStdString());
    ui->label->setText(label);
}

std::string NodeBox::getLabel() const
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return "";
    }
    NodeStatePtr state = nh->getNodeState();
    return state->getLabel();
}

void NodeBox::registerEvent(Connectable* c)
{
    if(c->isOutput()) {
        registerOutputEvent(dynamic_cast<Output*>(c));
    } else {
        registerInputEvent(dynamic_cast<Input*>(c));
    }
}

void NodeBox::unregisterEvent(Connectable*)
{
}

void NodeBox::registerInputEvent(Input* /*in*/)
{
    Q_EMIT changed(this);
}

void NodeBox::registerOutputEvent(Output* out)
{
    apex_assert_hard(out);

    Q_EMIT changed(this);
}

void NodeBox::resizeEvent(QResizeEvent */*e*/)
{
    Q_EMIT changed(this);
}

void NodeBox::init()
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }

    NodeStatePtr state = nh->getNodeState();
    Point pt = state->getPos();
    move(QPoint(pt.x, pt.y));
    (*state->pos_changed)();
}

Port* NodeBox::createPort(ConnectableWeakPtr connector, QBoxLayout *layout)
{
    apex_assert_hard(QApplication::instance()->thread() == QThread::currentThread());

    Port* port = new Port(connector);

    port->setFlipped(isFlipped());
    port->setMinimizedSize(isMinimizedSize());

    QObject::connect(this, SIGNAL(minimized(bool)), port, SLOT(setMinimizedSize(bool)));
    QObject::connect(this, SIGNAL(flipped(bool)), port, SLOT(setFlipped(bool)));

    ConnectablePtr adaptee = port->getAdaptee().lock();
    if(adaptee) {
        layout->addWidget(port);
    }

    QObject::connect(port, &Port::destroyed, [this, port](QObject* o) {
        portRemoved(port);
    });

    portAdded(port);

    return port;
}

bool NodeBox::eventFilter(QObject* o, QEvent* e)
{
    if(o == this) {
        if(e->type() == QEvent::MouseButtonDblClick) {
            if(hasSubGraph()) {
                Q_EMIT showSubGraphRequest(getSubGraph()->getAbsoluteUUID());
                return true;
            }
        }
        return false;
    }

    if(o == ui->label) {
        QMouseEvent* em = dynamic_cast<QMouseEvent*>(e);
        if(e->type() == QEvent::MouseButtonDblClick && em->button() == Qt::LeftButton) {
            Q_EMIT renameRequest(this);
            e->accept();

            return true;
        }
    } else if(grip_ && o == grip_) {
        if(e->type() == QEvent::MouseButtonPress) {
            adapter_->setManualResize(true);
        } else if(e->type() == QEvent::MouseButtonRelease) {
            adapter_->setManualResize(false);
        }
    }

    return false;
}

void NodeBox::enabledChangeEvent(bool val)
{
    setProperty("disabled", !val);

    ui->enablebtn->blockSignals(true);
    ui->enablebtn->setChecked(val);
    ui->enablebtn->blockSignals(false);

    refreshStylesheet();
}

QString NodeBox::getNodeState()
{
    NodeWorkerPtr worker = node_worker_.lock();
    if(!worker) {
        return "";
    }

    QString state;
    switch(worker->getState()) {
    case NodeWorker::State::IDLE:
        state = "idle"; break;
    case NodeWorker::State::ENABLED:
        state = "enabled"; break;
    case NodeWorker::State::FIRED:
        state = "fired"; break;
    case NodeWorker::State::PROCESSING:
        state = "processing"; break;
    default:
        state = "unknown"; break;
    }

    return state;
}

void NodeBox::paintEvent(QPaintEvent* /*e*/)
{
    NodeWorkerPtr worker = node_worker_.lock();
    if(!adapter_) {
        return;
    }
    QString state = getNodeState();

    QString transition_state;
    if(worker) {
        NodeHandlePtr handle = worker->getNodeHandle();
        OutputTransition* ot = handle->getOutputTransition();
        InputTransition* it = handle->getInputTransition();

        transition_state += ", it: ";
        transition_state += it->isEnabled() ? "enabled" : "disabled";
        transition_state += ", ot: ";
        transition_state += ot->isEnabled() ? "enabled" : "disabled";
    }
    info_exec->setText(QString("<img src=\":/node_") + state + ".png\" />");
    info_exec->setToolTip(state + transition_state);

    bool is_error = false;
    bool is_warn = false;
    if(worker) {
        is_error = worker->isError() && worker->errorLevel() == ErrorState::ErrorLevel::ERROR;
        is_warn = worker->isError() && worker->errorLevel() == ErrorState::ErrorLevel::WARNING;
    }

    bool error_change = ui->boxframe->property("error").toBool() != is_error;
    bool warning_change = ui->boxframe->property("warning").toBool() != is_warn;

    setProperty("error", is_error);
    setProperty("warning", is_warn);

    if(error_change || warning_change) {
        if(is_error) {
            QString msg = QString::fromStdString(worker->errorMessage());
            setToolTip(msg);
            info_error->setToolTip(msg);
            info_error->setVisible(true);
        } else if(is_warn) {
            QString msg = QString::fromStdString(worker->errorMessage());
            setToolTip(msg);
            info_error->setToolTip(msg);
            info_error->setVisible(true);
        } else {
            setToolTip(ui->label->text());
            info_error->setVisible(false);
        }

        refreshStylesheet();
    }

    if(!initialized_) {
        adjustSize();
        initialized_ = true;
    }
}

void NodeBox::moveEvent(QMoveEvent* e)
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }

    eventFilter(this, e);
}

void NodeBox::triggerPlaced()
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }

    Point p;
    p.x = pos().x();
    p.y = pos().y();
    nh->getNodeState()->setPos(p);
}

void NodeBox::setSelected(bool selected)
{
    setProperty("focused",selected);
    refreshStylesheet();
}

void NodeBox::keyPressEvent(QKeyEvent *)
{

}

void NodeBox::stop()
{
    QObject::disconnect(this);
    adapter_->stop();
}

void NodeBox::getInformation()
{
    Q_EMIT helpRequest(this);
}

void NodeBox::refreshStylesheet()
{
    apex_assert_hard(QThread::currentThread() == QApplication::instance()->thread());
    for(auto* child : children()) {
        if(QWidget* widget = dynamic_cast<QWidget*>(child)) {
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            widget->update();
        }
    }

    style()->unpolish(this);
    style()->polish(this);
    update();
}

void NodeBox::showProfiling(bool show)
{
    NodeWorkerPtr node = node_worker_.lock();
    if(node->isProfiling() != show) {
        node->setProfiling(show);
    }
}

void NodeBox::killContent()
{
    NodeWorkerPtr worker = node_worker_.lock();
    if(!worker) {
        return;
    }
    worker->killExecution();
}

void NodeBox::triggerFlipSides()
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }

    NodeStatePtr state = nh->getNodeState();
    bool flip = state->isFlipped();
    Q_EMIT flipped(flip);
}

void NodeBox::triggerMinimized()
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }

    NodeStatePtr state = nh->getNodeState();
    bool minimize = state->isMinimized();
    Q_EMIT minimized(minimize);
}

void NodeBox::updateVisuals()
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return;
    }
    NodeStatePtr state = nh->getNodeState();
    bool flip = state->isFlipped();


    if(grip_) {
        auto* layout = dynamic_cast<QGridLayout*>(ui->boxframe->layout());
        if(layout) {
            if(flip) {
                layout->addWidget(grip_, 3, 0, Qt::AlignBottom | Qt::AlignLeft);
            } else {
                layout->addWidget(grip_, 3, 2, Qt::AlignBottom | Qt::AlignRight);
            }
        }
    }

    setProperty("flipped", flip);
    ui->boxframe->setLayoutDirection(flip ? Qt::RightToLeft : Qt::LeftToRight);
    ui->frame->setLayoutDirection(Qt::LeftToRight);

    bool flag_set = ui->boxframe->property("content_minimized").toBool();
    bool minimized = isMinimizedSize();

    if(minimized != flag_set) {
        setProperty("content_minimized", minimized);

        if(minimized) {
            ui->frame->hide();
            info_exec->hide();
            ui->input_panel->hide();
            ui->output_panel->hide();
            ui->slot_panel->hide();
            ui->trigger_panel->hide();

            if(grip_) {
                grip_->hide();
            }

            ui->gridLayout->removeWidget(ui->enablebtn);
            ui->gridLayout->addWidget(ui->enablebtn, 2, 0);

            ui->header_spacer->changeSize(0, 0);

        } else {
            ui->header_spacer->changeSize(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);
            ui->header_spacer->invalidate();

            ui->gridLayout->removeWidget(ui->enablebtn);
            ui->gridLayout->addWidget(ui->enablebtn, 1, 0);

            ui->frame->show();
            info_exec->show();
            ui->output_panel->show();
            ui->input_panel->show();
            ui->output_panel->show();
            ui->slot_panel->show();
            ui->trigger_panel->show();

            if(grip_) {
                grip_->show();
            }

        }
        layout()->invalidate();
        QApplication::processEvents();
        adjustSize();
    }


    QColor text_color = Qt::black;

    int r, g, b;
    state->getColor(r, g, b);

    QString style = parent_ ? parent_->styleSheet() : styleSheet();

    if(r >= 0 && g >= 0 && b >= 0) {
        QColor background(r,g,b);

        bool light = (background.lightness() > 128);

        QColor border = light ? background.darker(160) : background.lighter(160);
        QColor background_selected = light ? background.darker(140) : background.lighter(140);
        QColor border_selected = light ? border.darker(140) : border.lighter(140);
        text_color = light ? Qt::black: Qt::white;

        style += "csapex--NodeBox QFrame#boxframe { ";
        style += "background-color: rgb(" + QString::number(background.red()) + ", " +
                QString::number(background.green()) + ", " +
                QString::number(background.blue()) + ");";
        style += "border-color: rgb(" + QString::number(border.red()) + ", " +
                QString::number(border.green()) + ", " +
                QString::number(border.blue()) + ");";
        style += "}";
        style += "csapex--NodeBox[focused=\"true\"] QFrame#boxframe { ";
        style += "background-color: rgb(" + QString::number(background.red()) + ", " +
                QString::number(background_selected.green()) + ", " +
                QString::number(background_selected.blue()) + ");";
        style += "border-color: rgb(" + QString::number(border.red()) + ", " +
                QString::number(border_selected.green()) + ", " +
                QString::number(border_selected.blue()) + ");";
        style += "}";
    }

    style += "csapex--NodeBox QLabel, csapex--NodeBox QGroupBox { ";
    style += "color: rgb(" + QString::number(text_color.red()) + ", " +
            QString::number(text_color.green()) + ", " +
            QString::number(text_color.blue()) + ") !important;";
    style += "}";

    setStyleSheet(style);

    refreshStylesheet();

    QApplication::processEvents();
    adjustSize();
}

bool NodeBox::isMinimizedSize() const
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return false;
    }
    NodeStatePtr state = nh->getNodeState();
    return state->isMinimized();
}

bool NodeBox::isFlipped() const
{
    NodeHandlePtr nh = node_handle_.lock();
    if(!nh) {
        return false;
    }
    NodeStatePtr state = nh->getNodeState();
    return state->isFlipped();
}

bool NodeBox::hasSubGraph() const
{
    return dynamic_cast<Graph*>(getNode()) != nullptr;
}

GraphFacade* NodeBox::getSubGraph() const
{
    NodeHandlePtr nh = node_handle_.lock();
    if(nh) {
        NodePtr node = nh->getNode().lock();
        if(node) {
            return parent_->getGraphFacade()->getSubGraph(node->getUUID());
            //            Graph::Ptr graph = std::dynamic_pointer_cast<Graph>(node);
            //            if(graph) {
            //                return graph;
            //            }
        }
    }

    throw std::logic_error("Called getSubGraph() on an invalid node. "
                           "Check with hasSubGraph().");
}

void NodeBox::nodeStateChangedEvent()
{
    NodeWorkerPtr worker = node_worker_.lock();
    if(!worker) {
        return;
    }
    NodeStatePtr state = worker->getNodeHandle()->getNodeState();

    bool state_enabled = state->isEnabled();
    bool worker_enabled = worker->isProcessingEnabled();
    if(state_enabled != worker_enabled) {
        worker->setProcessingEnabled(state_enabled);
        ui->label->setEnabled(state_enabled);
        enabledChange(state_enabled);
    }

    setLabel(state->getLabel());
    ui->label->setToolTip(QString::fromStdString(worker->getUUID().getFullName()));

    updateThreadInformation();

    updateVisuals();

    auto pt = state->getPos();
    move(QPoint(pt.x, pt.y));
}
/// MOC
#include "../../../include/csapex/view/node/moc_box.cpp"
