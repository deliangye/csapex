/// HEADER
#include <csapex/connection_type_manager.h>

using namespace csapex;

ConnectionTypeManager::ConnectionTypeManager()
{
}

ConnectionType::Ptr ConnectionTypeManager::createMessage(const std::string& type)
{
    if(instance().classes.empty()) {
        throw std::runtime_error("no connection types registered!");
    }

    return instance().classes[type]();
}

void ConnectionTypeManager::registerMessage(const std::string &type, Constructor constructor)
{
    instance().classes[type] = constructor;
}
