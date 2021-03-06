#ifndef SET_LOGGER_LEVEL_H
#define SET_LOGGER_LEVEL_H

/// COMPONENT
#include "command_impl.hpp"
#include <csapex/utility/uuid.h>
#include <csapex/model/execution_mode.h>

namespace csapex
{
namespace command
{

class CSAPEX_COMMAND_EXPORT SetLoggerLevel : public CommandImplementation<SetLoggerLevel>
{
    COMMAND_HEADER(SetLoggerLevel);

public:
    SetLoggerLevel(const AUUID &graph_uuid, const UUID& node, int level);

    virtual std::string getDescription() const override;

    void serialize(SerializationBuffer &data) const override;
    void deserialize(SerializationBuffer& data) override;

protected:
    bool doExecute() override;
    bool doUndo() override;
    bool doRedo() override;

private:
    UUID uuid;
    int was_level;
    int level;
};

}

}
#endif // SET_LOGGER_LEVEL_H

