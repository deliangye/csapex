#ifndef MARKER_MESSAGE_H
#define MARKER_MESSAGE_H

/// COMPONENT
#include <csapex/msg/message.h>

namespace csapex
{
namespace connection_types
{

struct MarkerMessage : public Message
{
public:
    typedef std::shared_ptr<MarkerMessage> Ptr;

protected:
    MarkerMessage(const std::string &name, Stamp stamp);

public:
    bool canConnectTo(const ConnectionType* other_side) const override;
    bool acceptsConnectionFrom(const ConnectionType* other_side) const override;
};
template <>
struct type<MarkerMessage> {
    static std::string name() {
        return "Marker";
    }
};

}
}

#endif // MARKER_MESSAGE_H
