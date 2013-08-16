#ifndef COMMAND_DELETE_BOX_H
#define COMMAND_DELETE_BOX_H

/// COMPONENT
#include "command.h"
#include "memento.h"

/// SYSTEM
#include <QPoint>
#include <QWidget>

namespace csapex
{

class Box;

namespace command
{
class DeleteBox : public Command
{
public:
    DeleteBox(Box* box);

protected:
    bool execute();
    bool undo();
    bool redo();

    void refresh();

protected:
    Box* box;

    Graph* graph;
    QWidget* parent;
    QPoint pos;

    std::string type;
    std::string uuid;

    Command::Ptr remove_connections;

    Memento::Ptr saved_state;
};

}
}
#endif // COMMAND_DELETE_BOX_H
