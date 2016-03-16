#ifndef PARAM_ADAPTER_H
#define PARAM_ADAPTER_H

/// PROJECT
#include <csapex/utility/slim_signal.h>
#include <csapex/param/parameter.h>
#include <csapex/command/command_fwd.h>

/// SYSTEM
#include <string>
#include <QObject>

class QBoxLayout;
class QHBoxLayout;


namespace csapex
{

class ParameterContextMenu;

class ParameterAdapter : public QObject
{
    Q_OBJECT

public:
    ParameterAdapter(param::Parameter::Ptr p);
    virtual ~ParameterAdapter();

    void doSetup(QBoxLayout* layout, const std::string& display_name);

protected:
    virtual void setup(QBoxLayout* layout, const std::string& display_name) = 0;

public:
    csapex::slim_signal::Signal<void(CommandPtr)> executeCommand;

Q_SIGNALS:
    void modelCallback(std::function<void()>);

public Q_SLOTS:
    void executeModelCallback(std::function<void()>);

protected:
    void connectInGuiThread(csapex::slim_signal::Signal<void(csapex::param::Parameter*)>& signal,
                 std::function<void()> cb);

protected:
    param::Parameter::Ptr p_;

    ParameterContextMenu* context_handler;

private:
    std::vector<csapex::slim_signal::ScopedConnection> connections;

};


}

#endif // PARAM_ADAPTER_H