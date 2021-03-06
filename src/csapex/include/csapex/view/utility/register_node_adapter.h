#ifndef REGISTER_NODE_ADAPTER_H
#define REGISTER_NODE_ADAPTER_H

/// COMPONENT
#include <csapex/model/node_facade_local.h>
#include <csapex/model/node_handle.h>
#include <csapex/utility/register_apex_plugin.h>
#include <csapex/view/node/node_adapter_builder.h>

#define MAKE_CLASS(NS, C) NS::C##Builder

#define CSAPEX_REGISTER_NODE_ADAPTER_NS(Namespace, Adapter, Adaptee) \
namespace Namespace {\
class Adapter##Builder : public csapex::NodeAdapterBuilder \
{ \
public: \
    virtual std::string getWrappedType() const \
    { \
        return #Adaptee; \
    } \
    virtual csapex::NodeAdapterPtr build(csapex::NodeFacadePtr facade, NodeBox* parent) const \
    { \
        return std::make_shared<Adapter>(facade, parent); \
    } \
}; \
}\
CSAPEX_REGISTER_CLASS(MAKE_CLASS(Namespace,Adapter),csapex::NodeAdapterBuilder)

#define CSAPEX_REGISTER_LEGACY_NODE_ADAPTER_NS(Namespace, Adapter, Adaptee) \
namespace Namespace {\
class Adapter##Builder : public csapex::NodeAdapterBuilder \
{ \
public: \
    virtual std::string getWrappedType() const \
    { \
        return #Adaptee; \
    } \
    virtual csapex::NodeAdapterPtr build(csapex::NodeFacadePtr facade, NodeBox* parent) const \
    { \
        auto lf = std::dynamic_pointer_cast<csapex::NodeFacadeLocal>(facade);\
        apex_assert_hard(lf);\
        std::weak_ptr<Adaptee> adaptee = std::dynamic_pointer_cast<Adaptee> (lf->getNode()); \
        return std::make_shared<Adapter>(facade, parent, adaptee); \
    } \
}; \
}\
CSAPEX_REGISTER_CLASS(MAKE_CLASS(Namespace,Adapter),csapex::NodeAdapterBuilder)

#define CSAPEX_REGISTER_NODE_ADAPTER(Adapter, Adaptee) \
CSAPEX_REGISTER_NODE_ADAPTER_NS(csapex,Adapter,Adaptee)

#define CSAPEX_REGISTER_LEGACY_NODE_ADAPTER(Adapter, Adaptee) \
CSAPEX_REGISTER_LEGACY_NODE_ADAPTER_NS(csapex,Adapter,Adaptee)

#endif // REGISTER_NODE_ADAPTER_H
