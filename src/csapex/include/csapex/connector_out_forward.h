#ifndef CONNECTOR_OUT_FORWARD_H
#define CONNECTOR_OUT_FORWARD_H

/// COMPONENT
#include "connector_out.h"

namespace csapex
{

class ConnectorOutForward : public ConnectorOut
{
public:
    ConnectorOutForward(Box* parent, const std::string& uuid);
    ConnectorOutForward(Box* parent, int sub_id);

    virtual bool isForwarding() const;
};

}

#endif // CONNECTOR_OUT_FORWARD_H
