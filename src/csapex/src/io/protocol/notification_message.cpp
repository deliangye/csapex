/// HEADER
#include <csapex/io/protcol/notification_message.h>

/// PROJECT
#include <csapex/serialization/broadcast_message_serializer.h>
#include <csapex/utility/uuid_provider.h>
#include <csapex/serialization/serialization_buffer.h>

/// SYSTEM
#include <iostream>

CSAPEX_REGISTER_BROADCAST_SERIALIZER(NotificationMessage)

using namespace csapex;

NotificationMessage::NotificationMessage(const Notification &notification)
    : notification(notification)
{

}

NotificationMessage::NotificationMessage()
{

}

void NotificationMessage::serialize(SerializationBuffer &data) const
{
//    std::cerr << "serializing Notification" << std::endl;
    data << notification.auuid;

    data << notification.error;
    data << notification.message;
}

void NotificationMessage::deserialize(SerializationBuffer& data)
{
//    std::cerr << "deserializing Notification" << std::endl;
    data >> notification.auuid;

//    std::cerr << "full name: " << full_name << std::endl;

    data >> notification.error;
//    std::cerr << "error level: " << (int) notification.error << std::endl;

    data >> notification.message;
//    std::cerr << "message: " << notification.message.str() << std::endl;
}

const Notification& NotificationMessage::getNotification() const
{
    return notification;
}
