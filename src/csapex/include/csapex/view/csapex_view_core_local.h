#ifndef CSAPEX_VIEW_CORE_LOCAL_H
#define CSAPEX_VIEW_CORE_LOCAL_H

/// COMPONENT
#include <csapex/view/csapex_view_core.h>

namespace csapex
{
class CSAPEX_QT_EXPORT CsApexViewCoreLocal : public CsApexViewCore
{
public:
    CsApexViewCoreLocal(CsApexCorePtr core);
    CsApexViewCoreLocal(CsApexViewCore &parent, ExceptionHandler &exception_handler);

    void sendNotification(const std::string& notification, ErrorState::ErrorLevel error_level = ErrorState::ErrorLevel::ERROR) override;

    // CORE
    void reset() override;
    void load(const std::string& file) override;
    void saveAs(const std::string& file, bool quiet = false) override;

    void setPause(bool paused) override;
    bool isPaused() const override;

    bool isSteppingMode() const override;
    void setSteppingMode(bool stepping) override;
    void step() override;

    Settings& getSettings() const override;

    // TODO: add proxies or remove
    ExceptionHandler& getExceptionHandler() const override;
    GraphFacadePtr getRoot() override;
    void shutdown() override;
    void clearBlock() override;
    void resetActivity() override;

    ThreadPoolPtr getThreadPool() override;

    CommandExecutorPtr getCommandDispatcher() override;

    PluginLocatorPtr getPluginLocator() const override;
    NodeFactoryPtr getNodeFactory() const override;
    SnippetFactoryPtr getSnippetFactory() const override;

    ProfilerPtr getProfiler() const override;

    std::shared_ptr<DragIO> getDragIO() override;
    std::shared_ptr<NodeAdapterFactory> getNodeAdapterFactory() override;

private:
    CsApexCorePtr core_;

    std::shared_ptr<NodeAdapterFactory> node_adapter_factory_;
    std::shared_ptr<CommandExecutor> dispatcher_;

    std::shared_ptr<NodeFactory> node_factory_;
    std::shared_ptr<SnippetFactory> snippet_factory_;

    std::shared_ptr<DragIO> drag_io;

    ExceptionHandler &exception_handler_;
};

}

#endif // CSAPEX_VIEW_CORE_LOCAL_H
