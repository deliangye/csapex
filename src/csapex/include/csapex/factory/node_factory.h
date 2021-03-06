#ifndef NODE_FACTORY_H
#define NODE_FACTORY_H

/// COMPONENT
#include <csapex/model/node_constructor.h>
#include <csapex/utility/uuid.h>

/// PROJECT
#include <csapex/utility/notifier.h>
#include <csapex/core/core_fwd.h>
#include <csapex/model/model_fwd.h>
#include <csapex/plugin/plugin_fwd.h>
#include <csapex/csapex_export.h>

/// SYSTEM
#include <vector>
#include <csapex/utility/slim_signal.h>
#include <unordered_map>

class TiXmlElement;

namespace csapex
{

class CSAPEX_EXPORT NodeFactory : public Notifier
{
public:
    typedef std::shared_ptr<NodeFactory> Ptr;

public:
    NodeFactory(Settings& settings, PluginLocator *locator);
    ~NodeFactory();

    NodeFactory(const NodeFactory&) = delete;
    NodeFactory& operator = (const NodeFactory&) = delete;

    void loadPlugins();
    void shutdown();

public:
    void registerNodeType(NodeConstructor::Ptr provider, bool suppress_signals = false);

    bool isValidType(const std::string& type) const;

    NodeConstructorPtr getConstructor(const std::string& type);
    std::vector<NodeConstructorPtr> getConstructors();

    NodeFacadePtr makeNode(const std::string& type, const UUID& uuid, const UUIDProviderPtr &uuid_provider);
    NodeFacadePtr makeNode(const std::string& type, const UUID& uuid, const UUIDProviderPtr &uuid_provider, NodeStatePtr state);

    NodeFacadeLocalPtr makeNodeLocal(const std::string& type, const UUID& uuid, const UUIDProviderPtr &uuid_provider);
    NodeFacadeLocalPtr makeNodeLocal(const std::string& type, const UUID& uuid, const UUIDProviderPtr &uuid_provider, NodeStatePtr state);

    std::map<std::string, std::vector<NodeConstructor::Ptr> > getTagMap();

public:
    slim_signal::Signal<void(const std::string&)> loaded;
    slim_signal::Signal<void()> new_node_type;
    slim_signal::Signal<void(NodeFacadePtr)> node_constructed;
    slim_signal::Signal<void(const std::string& file, const TiXmlElement* document)> manifest_loaded;

protected:
    void ensureLoaded();
    void rebuildPrototypes();
    void rebuildMap();

protected:
    Settings &settings_;
    csapex::PluginLocator* plugin_locator_;

    std::map<std::string, std::vector<NodeConstructor::Ptr> > tag_map_;
    std::vector<NodeConstructor::Ptr> constructors_;

    std::shared_ptr<PluginManager<Node>> node_manager_;

    bool tag_map_has_to_be_rebuilt_;
};

}

#endif // NODE_FACTORY_H
