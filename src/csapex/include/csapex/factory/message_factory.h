#ifndef MESSAGE_FACTORY_H
#define MESSAGE_FACTORY_H

/// COMPONENT
#include <csapex/model/token_data.h>
#include <csapex/msg/message.h>
#include <csapex/msg/token_traits.h>

/// PROJECT
#include <csapex/utility/singleton.hpp>

/// SYSTEM
#include <map>
#include <string>
#include <functional>
#include <boost/type_traits.hpp>
#include <typeindex>

namespace YAML {
class Emitter;
}

namespace csapex {

class CSAPEX_EXPORT MessageFactory : public Singleton<MessageFactory>
{
    friend class Singleton<MessageFactory>;

public:
    typedef std::function<TokenData::Ptr()>  Constructor;

public:
    bool isMessageRegistered(const std::string& type) const;

    template <typename M>
    static TokenData::Ptr createMessage() {
        return connection_types::makeEmpty<M>();
    }
    template <template <typename> class Wrapper,typename M>
    static TokenData::Ptr createDirectMessage() {
        return connection_types::makeEmptyMessage< Wrapper<M> >();
    }

    static TokenData::Ptr createMessage(const std::string& type);

    static TokenData::Ptr readMessage(const std::string& path);
    static void writeMessage(const std::string& path, const TokenData &msg);
    static void writeMessage(YAML::Emitter &yaml,
                             const TokenData &msg);

    void shutdown() override;

public:
    template <template <typename> class Wrapper, typename M>
    static void registerDirectMessage()
    {
        MessageFactory& instance = MessageFactory::instance();
        std::string type = connection_types::serializationName< Wrapper<M> >();
        if(!instance.isMessageRegistered(type)) {
            instance.registerMessage(type,
                                     std::type_index(typeid(Wrapper<M>)),
                                     std::bind(&MessageFactory::createDirectMessage<Wrapper, M>));
        }
    }

    template <typename M>
    static void registerMessage() {
        MessageFactory::instance().registerMessage(connection_types::serializationName<M>(),
                                                   std::type_index(typeid(M)),
                                                   std::bind(&MessageFactory::createMessage<M>));
    }

private:
    MessageFactory();

    static void registerMessage(std::string type, std::type_index typeindex, Constructor constructor);

private:
    std::map<std::string, Constructor> type_to_constructor;
    std::map<std::string, std::type_index> type_to_type_index;
};



template <typename T>
struct MessageConstructorRegistered
{
    MessageConstructorRegistered() {
        csapex::MessageFactory::registerMessage<T>();
    }
};

template <template <typename> class Wrapper, typename T>
struct DirectMessageConstructorRegistered
{
    DirectMessageConstructorRegistered() {
        csapex::MessageFactory::registerDirectMessage<Wrapper, T>();
    }
};

}

#endif // MESSAGE_FACTORY_H
