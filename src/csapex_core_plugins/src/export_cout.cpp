/// HEADER
#include "export_cout.h"

/// PROJECT
#include <csapex/model/box.h>
#include <csapex/model/connector_in.h>
#include <csapex/utility/stream_interceptor.h>
#include <csapex/model/message.h>

/// SYSTEM
#include <pluginlib/class_list_macros.h>

PLUGINLIB_EXPORT_CLASS(csapex::ExportCout, csapex::BoxedObject)

using namespace csapex;

ExportCout::ExportCout()
    : connector_(NULL)
{
    addTag(Tag::get("ConsoleIO"));
    addTag(Tag::get("General"));
    addTag(Tag::get("Output"));
    setIcon(QIcon(":/terminal.png"));
}

void ExportCout::fill(QBoxLayout *layout)
{
    if(connector_ == NULL) {
        connector_ = new ConnectorIn(box_, 0);
        connector_->setLabel("Anything");
        connector_->setType(connection_types::AnyMessage::make());

        box_->setSynchronizedInputs(true);
        box_->addInput(connector_);
    }
}

void ExportCout::allConnectorsArrived()
{
    ConnectionType::Ptr msg = connector_->getMessage();

    std::cout << "writing to cout: ";
    msg->write(std::cout);
    std::cout << std::endl;

    StreamInterceptor::instance().cout << *msg << "\n---" << std::endl;
}
