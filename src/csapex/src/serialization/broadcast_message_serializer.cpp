/// HEADER
#include <csapex/serialization/broadcast_message_serializer.h>

/// PROJECT
#include <csapex/serialization/packet_serializer.h>
#include <csapex/utility/assert.h>
#include <csapex/serialization/serialization_buffer.h>

/// SYSTEM
#include <iostream>

using namespace csapex;

SerializerRegistered<BroadcastMessageSerializer> g_register_BroadcastMessage_serializer_(BroadcastMessage::PACKET_TYPE_ID, &BroadcastMessageSerializer::instance());


BroadcastMessageSerializerInterface::~BroadcastMessageSerializerInterface()
{

}

void BroadcastMessageSerializer::serialize(const SerializableConstPtr &packet, SerializationBuffer& data)
{
    if(const BroadcastMessageConstPtr& cmd = std::dynamic_pointer_cast<BroadcastMessage const>(packet)) {
//        std::cerr << "serializing BroadcastMessage" << std::endl;
        std::string type = cmd->getType();
        auto it = serializers_.find(type);
        if(it != serializers_.end()) {

//            std::cerr << "serializing BroadcastMessage (type=" << type << ")" << std::endl;
            data << type;

            // defer serialization to the corresponding serializer
            std::shared_ptr<BroadcastMessageSerializerInterface> serializer = it->second;
            serializer->serialize(cmd, data);

        } else {
            std::cerr << "cannot serialize BroadcastMessage of type " << type << ", none of the " << serializers_.size() << " serializers matches." << std::endl;
        }

    }
}

SerializablePtr BroadcastMessageSerializer::deserialize(SerializationBuffer& data)
{
//    std::cerr << "deserializing BroadcastMessage" << std::endl;

    std::string type;
    data >> type;

    auto it = serializers_.find(type);
    if(it != serializers_.end()) {

//        std::cerr << "deserializing BroadcastMessage (type=" << type << ")" << std::endl;

        // defer serialization to the corresponding serializer
        std::shared_ptr<BroadcastMessageSerializerInterface> serializer = it->second;
        return serializer->deserialize(data);

    } else {
        std::cerr << "cannot deserialize BroadcastMessage of type " << type << ", none of the " << serializers_.size() << " serializers matches." << std::endl;
    }


    return BroadcastMessagePtr();
}

void BroadcastMessageSerializer::registerSerializer(const std::string &type, std::shared_ptr<BroadcastMessageSerializerInterface> serializer)
{
//    std::cout << "registering serializer of type " << type << std::endl;
    instance().serializers_[type] = serializer;
}
