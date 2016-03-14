/// HEADER
#include <csapex/view/param/param_adapter.h>

/// PROJECT
#include <csapex/utility/assert.h>
#include <csapex/view/node/parameter_context_menu.h>

/// SYSTEM
#include <QApplication>
#include <QThread>

using namespace csapex;

namespace {
void assertGuiThread()
{
    apex_assert_hard(QThread::currentThread() == QApplication::instance()->thread());
}

void assertNotGuiThread()
{
    apex_assert_hard(QThread::currentThread() != QApplication::instance()->thread());
}
}

ParameterAdapter::ParameterAdapter(param::Parameter::Ptr p)
    : p_(p)
{
    apex_assert_hard(p);

    qRegisterMetaType<std::function<void()>>("std::function<void()>");

    QObject::connect(this, &ParameterAdapter::modelCallback,
                     this, &ParameterAdapter::executeModelCallback);

    context_handler = new ParameterContextMenu(p);
}

ParameterAdapter::~ParameterAdapter()
{
}

void ParameterAdapter::doSetup(QBoxLayout *layout, const std::string &display_name)
{
    setup(layout, display_name);
}

void ParameterAdapter::connectInGuiThread(csapex::slim_signal::Signal<void (csapex::param::Parameter *)> &signal,
                                                  std::function<void ()> cb)
{
    // cb should be executed in the gui thread
    connections.push_back(signal.connect(std::bind(&ParameterAdapter::modelCallback, this, cb)));
}

void ParameterAdapter::executeModelCallback(std::function<void()> cb)
{
    assertGuiThread();
    cb();
}


/// MOC
#include "../../../include/csapex/view/param/moc_param_adapter.cpp"
