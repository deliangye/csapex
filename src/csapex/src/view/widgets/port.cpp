/// HEADER
#include <csapex/view/widgets/port.h>

/// COMPONENT
#include <csapex/model/node.h>
#include <csapex/command/dispatcher.h>
#include <csapex/command/add_connection.h>
#include <csapex/command/command_factory.h>
#include <csapex/msg/input.h>
#include <csapex/msg/static_output.h>
#include <csapex/view/designer/graph_view.h>
#include <csapex/view/widgets/message_preview_widget.h>
#include <csapex/view/designer/designer_scene.h>
#include <csapex/signal/slot.h>
#include <csapex/csapex_mime.h>

/// SYSTEM
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <QDrag>
#include <QWidget>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QHelpEvent>
#include <QEvent>
#include <QInputDialog>
#include <QApplication>
#include <QTimer>

using namespace csapex;

Q_DECLARE_METATYPE(ConnectorPtr)

Port::Port(QWidget *parent)
    : QFrame(parent),
      refresh_style_sheet_(false), minimized_(false), flipped_(false), hovered_(false),
      buttons_down_(0)
{
    setFlipped(flipped_);

    setFocusPolicy(Qt::NoFocus);
    setAcceptDrops(true);

    setContextMenuPolicy(Qt::PreventContextMenu);

    setMinimizedSize(minimized_);

    setEnabled(true);

    double_click_timer_ = new QTimer(this);

    QObject::connect(double_click_timer_, &QTimer::timeout, this, &Port::mouseClickEvent);

    double_click_timer_->setInterval(200);
}

Port::Port(ConnectorWeakPtr adaptee, QWidget *parent)
    : Port(parent)
{
    adaptee_ = adaptee;
    ConnectorPtr adaptee_ptr = adaptee_.lock();
    if(adaptee_ptr) {
        createToolTip();

        connections_.push_back(adaptee_ptr->enabled_changed.connect([this](bool e) { setEnabledFlag(e); }));
        connections_.push_back(adaptee_ptr->connectableError.connect([this](bool error,std::string msg,int level) { setError(error, msg, level); }));

        bool opt = dynamic_cast<Input*>(adaptee_ptr.get()) && dynamic_cast<Input*>(adaptee_ptr.get())->isOptional();
        setProperty("optional", opt);

        setProperty("type", QString::fromStdString(port_type::name(adaptee_ptr->getConnectorType())));

    } else {
        std::cerr << "creating empty port!" << std::endl;
    }
}


Port::~Port()
{
    for(auto& c : connections_) {
        c.disconnect();
    }
}

bool Port::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        createToolTip();
    }

    return QWidget::event(e);
}

bool Port::canOutput() const
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return false;
    }
    return adaptee->canOutput();
}

bool Port::canInput() const
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return false;
    }
    return adaptee->canInput();
}

bool Port::isOutput() const
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return false;
    }
    return adaptee->isOutput();
}

bool Port::isInput() const
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return false;
    }
    return adaptee->isInput();
}

ConnectorWeakPtr Port::getAdaptee() const
{
    return adaptee_;
}


void Port::paintEvent(QPaintEvent *e)
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return;
    }

    if(refresh_style_sheet_) {
        refresh_style_sheet_ = false;
        setStyleSheet(styleSheet());
    }
    QFrame::paintEvent(e);
}

void Port::refreshStylesheet()
{
    refresh_style_sheet_ = true;
}

void Port::setError(bool error, const std::string& msg)
{
    setError(error, msg, static_cast<int>(ErrorState::ErrorLevel::ERROR));
}

void Port::setError(bool error, const std::string& /*msg*/, int /*level*/)
{
    setProperty("error", error);
    refreshStylesheet();
}

void Port::setMinimizedSize(bool mini)
{
    minimized_ = mini;

    if(mini) {
        setFixedSize(8,8);
    } else {
        setFixedSize(16,16);
    }
}

bool Port::isMinimizedSize() const
{
    return minimized_;
}

void Port::setFlipped(bool flipped)
{
    flipped_ = flipped;
}

bool Port::isFlipped() const
{
    return flipped_;
}

bool Port::isHovered() const
{
    return hovered_;
}

void Port::setEnabledFlag(bool processing_enabled)
{
    bool enabled = processing_enabled;
    if(ConnectorPtr adaptee = adaptee_.lock()) {
        if(SlotPtr slot = std::dynamic_pointer_cast<Slot>(adaptee)) {
            if(slot->isActive()) {
                enabled = true;
            }
        }
    }
    setPortProperty("enabled", enabled);
    setPortProperty("disabled", !enabled);
    setEnabled(true);
    refreshStylesheet();
}

void Port::setPortProperty(const std::string& name, bool b)
{
    setProperty(name.c_str(), b);
}

void Port::createToolTip()
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return;
    }

    std::stringstream tooltip;
    tooltip << "UUID: " << adaptee->getUUID();
    tooltip << ", Type: " << adaptee->getType()->descriptiveName();
    if(InputPtr i = std::dynamic_pointer_cast<Input>(adaptee)) {
        if(TokenPtr token = i->getToken()) {
            tooltip << ", Last Message Type: " << token->getTokenData()->descriptiveName();
        }
    }
    tooltip << ", Connections: " << adaptee->getConnections().size();
    tooltip << ", Messages: " << adaptee->getCount();
    tooltip << ", Enabled: " << adaptee->isEnabled();
    tooltip << ", #: " << adaptee->sequenceNumber();
    tooltip << ", use_count: " << adaptee.use_count() - 1; // -1 for the local variable

    if(InputPtr in = std::dynamic_pointer_cast<Input>(adaptee)) {
        tooltip << ", optional: " << in->isOptional();

    }
    if(SlotPtr slot = std::dynamic_pointer_cast<Slot>(adaptee)) {
        tooltip << ", asynchronous: " << slot->isAsynchronous();
    }

    if(OutputPtr out = std::dynamic_pointer_cast<Output>(adaptee)) {
        tooltip << ", state: ";
        switch(out->getState()) {
        case Output::State::ACTIVE:
            tooltip << "ACTIVE";
            break;
        case Output::State::IDLE:
            tooltip << "IDLE";
            break;
        default:
            tooltip << "UNKNOWN";
        }
    }
    setToolTip(tooltip.str().c_str());
}

void Port::startDrag()
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return;
    }

    Q_EMIT mouseOut(this);

    bool left = (buttons_down_ & Qt::LeftButton) != 0;
    bool right = (buttons_down_ & Qt::RightButton) != 0;

    bool full_input = (adaptee->isInput() && adaptee->isConnected());
    bool create = left && !full_input;
    bool move = (right && adaptee->isConnected()) || (left && full_input);

    adaptee->connectionStart(adaptee);

    if(create || move) {
        QDrag* drag = new QDrag(this);
        QMimeData* mimeData = new QMimeData;

        if(move) {
            mimeData->setData(QString::fromStdString(csapex::mime::connection_move), QByteArray());
            mimeData->setProperty("Connector", qVariantFromValue(adaptee));

            drag->setMimeData(mimeData);

            drag->exec();

        } else {
            mimeData->setData(QString::fromStdString(csapex::mime::connection_create), QByteArray());
            mimeData->setProperty("Connector", qVariantFromValue(adaptee));

            drag->setMimeData(mimeData);

            drag->exec();
        }

        adaptee->connection_added_to(adaptee);
        buttons_down_ = Qt::NoButton;
    }
}

void Port::mouseMoveEvent(QMouseEvent* e)
{
    if(buttons_down_ == Qt::NoButton) {
        return;
    }

    startDrag();

    e->accept();
}

void Port::mouseDoubleClickEvent(QMouseEvent *e)
{
    e->accept();

    double_click_timer_->stop();

    auto adaptee = adaptee_.lock();

    if(adaptee) {
        bool ok = false;
        QString label = QInputDialog::getText(QApplication::activeWindow(), "Rename Port", "Enter a new name",
                                              QLineEdit::Normal, QString::fromStdString(adaptee->getLabel()), &ok);
        if(ok) {
            changePortRequest(label);
        }
    }
    buttons_down_ = e->buttons();
}

void Port::mouseClickEvent()
{
    startDrag();

    buttons_down_ = Qt::NoButton;
}

void Port::mouseReleaseEvent(QMouseEvent* e)
{
    double_click_timer_->setSingleShot(true);
    double_click_timer_->start();


    if(e->button() == Qt::MiddleButton) {
        Q_EMIT removeConnectionsRequest();
    }

    e->accept();
}

void Port::dragEnterEvent(QDragEnterEvent* e)
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return;
    }
    if(e->mimeData()->hasFormat(QString::fromStdString(csapex::mime::connection_create))) {
        ConnectorPtr from = e->mimeData()->property("Connector").value<ConnectorPtr>();
        if(from == adaptee) {
            return;
        }

        if(from->canConnectTo(adaptee.get(), false)) {
            if(adaptee->canConnectTo(from.get(), false)) {
                adaptee->connectionInProgress(adaptee, from);
                e->acceptProposedAction();
            }
        }
    } else if(e->mimeData()->hasFormat(QString::fromStdString(csapex::mime::connection_move))) {
        ConnectorPtr original = e->mimeData()->property("Connector").value<ConnectorPtr>();

        if(original->targetsCanBeMovedTo(adaptee.get())) {
            e->acceptProposedAction();
        }
    }
}

void Port::dragMoveEvent(QDragMoveEvent* e)
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return;
    }
    if(e->mimeData()->hasFormat(QString::fromStdString(csapex::mime::connection_create))) {
        e->acceptProposedAction();

    } else if(e->mimeData()->hasFormat(QString::fromStdString(csapex::mime::connection_move))) {
        ConnectorPtr from = e->mimeData()->property("Connector").value<ConnectorPtr>();

        from->connectionMovePreview(adaptee);

        e->acceptProposedAction();
    }
}

void Port::dropEvent(QDropEvent* e)
{
    ConnectorPtr adaptee = adaptee_.lock();
    if(!adaptee) {
        return;
    }
    if(e->mimeData()->hasFormat(QString::fromStdString(csapex::mime::connection_create))) {
        ConnectorPtr from = e->mimeData()->property("Connector").value<ConnectorPtr>();
        if(from && from != adaptee) {
            addConnectionRequest(from);
        }
    } else if(e->mimeData()->hasFormat(QString::fromStdString(csapex::mime::connection_move))) {
        ConnectorPtr from = e->mimeData()->property("Connector").value<ConnectorPtr>();
        if(from) {
            moveConnectionRequest(from);
            e->setDropAction(Qt::MoveAction);
        }
    }
}

void Port::enterEvent(QEvent */*e*/)
{
    hovered_ = true;
    Q_EMIT mouseOver(this);
}

void Port::leaveEvent(QEvent */*e*/)
{
    hovered_ = false;
    Q_EMIT mouseOut(this);
}

void Port::mousePressEvent(QMouseEvent* e)
{
    buttons_down_ = e->buttons();
}

/// MOC
#include "../../../include/csapex/view/widgets/moc_port.cpp"
