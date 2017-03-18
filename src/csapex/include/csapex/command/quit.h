#ifndef QUIT_H
#define QUIT_H

/// COMPONENT
#include "command.h"
#include <csapex/utility/uuid.h>
#include <csapex/utility/assert.h>

/// SYSTEM
#include <boost/any.hpp>

namespace csapex
{
namespace command
{

struct CSAPEX_COMMAND_EXPORT Quit : public Command
{
public:
    typedef std::shared_ptr<Quit> Ptr;

public:
    Quit();

    virtual std::string getType() const override;
    virtual std::string getDescription() const override;

    virtual bool isUndoable() const override;

protected:
    bool doExecute() override;
    bool doUndo() override;
    bool doRedo() override;

private:
    AUUID uuid;

    boost::any value;
};

}

}

#endif // QUIT_H