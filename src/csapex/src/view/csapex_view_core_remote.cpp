/// HEADER
#include <csapex/view/csapex_view_core_remote.h>


/// COMPONENT
#include <csapex/core/csapex_core.h>
#include <csapex/view/utility/message_renderer_manager.h>
#include <csapex/view/node/node_adapter_factory.h>
#include <csapex/view/designer/drag_io.h>
#include <csapex/command/dispatcher_remote.h>
#include <csapex/model/graph_facade.h>
#include <csapex/scheduling/thread_pool.h>
#include <csapex/command/dispatcher.h>
#include <csapex/io/session.h>
#include <csapex/io/protcol/core_requests.h>
#include <csapex/io/broadcast_message.h>
#include <csapex/io/protcol/notification_message.h>
#include <csapex/serialization/packet_serializer.h>
#include <csapex/view/gui_exception_handler.h>
#include <csapex/model/graph/graph_remote.h>
#include <csapex/model/graph/graph_local.h>
#include <csapex/model/graph_facade.h>

/// SYSTEM
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/version.hpp>
#if (BOOST_VERSION / 100000) >= 1 && (BOOST_VERSION / 100 % 1000) >= 54
namespace bf3 = boost::filesystem;
#else
namespace bf3 = boost::filesystem3;
#endif

using boost::asio::ip::tcp;
using namespace csapex;


CsApexViewCoreRemote::CsApexViewCoreRemote(const std::string &ip, int port, CsApexCorePtr core_tmp)
    : socket(io_service),
      resolver(io_service),
      resolver_iterator(boost::asio::connect(socket, resolver.resolve({ip, std::to_string(port)}))),
      session_(std::make_shared<Session>(std::move(socket))),
      exception_handler_(std::make_shared<GuiExceptionHandler>(false)),

      core_tmp_(core_tmp)
{
    session_->start();

    const auto& local_facade = core_tmp_->getRoot();
    std::shared_ptr<GraphRemote> remote_root_graph = std::make_shared<GraphRemote>(*std::dynamic_pointer_cast<GraphLocal>(local_facade->getGraph()));
    remote_root_ = std::make_shared<GraphFacade>(*local_facade->getThreadPool(),
                                                 remote_root_graph,
                                                 local_facade->getSubgraphNode(),
                                                 local_facade->getNodeFacade());

    // make the proxys only _after_ the session is started
    settings_ = std::make_shared<SettingsRemote>(session_);
    node_adapter_factory_ = std::make_shared<NodeAdapterFactory>(*settings_, core_tmp->getPluginLocator().get());
    dispatcher_ = std::make_shared<CommandDispatcherRemote>(session_);

    drag_io = std::make_shared<DragIO>(core_tmp->getPluginLocator(), dispatcher_.get());

    //    dispatcher_ = core_tmp_->getCommandDispatcher();
    node_factory_ = core_tmp_->getNodeFactory();
    snippet_factory_ = core_tmp_->getSnippetFactory();

    observe(session_->packet_received, [this](SerializableConstPtr packet){
        handlePacket(packet);
    });
    observe(session_->broadcast_received, [this](BroadcastMessageConstPtr packet){
        handleBroadcast(packet);
    });

    MessageRendererManager::instance().setPluginLocator(getPluginLocator());
    node_adapter_factory_->loadPlugins();

    running = true;
    spinner = std::thread([&](){
        while(running) {
            io_service.run();
        }
    });


    observe(core_tmp_->config_changed, config_changed);
    observe(core_tmp_->status_changed, status_changed);
    observe(core_tmp_->new_node_type, new_node_type);
    observe(core_tmp_->new_snippet_type, new_snippet_type);
    observe(core_tmp_->reset_requested, reset_requested);
    observe(core_tmp_->reset_done, reset_done);
    observe(core_tmp_->saved, saved);
    observe(core_tmp_->loaded, loaded);
    observe(core_tmp_->paused, paused);
    observe(core_tmp_->stepping_enabled, stepping_enabled);
    observe(core_tmp_->begin_step, begin_step);
    observe(core_tmp_->end_step, end_step);

    observe(core_tmp_->save_detail_request, save_detail_request);
    observe(core_tmp_->load_detail_request, load_detail_request);


    observe(dispatcher_->state_changed, undo_state_changed);
    observe(dispatcher_->dirty_changed, undo_dirty_changed);
}

CsApexViewCoreRemote::~CsApexViewCoreRemote()
{
    if(spinner.joinable()) {
        spinner.join();
    }
}


void CsApexViewCoreRemote::handlePacket(SerializableConstPtr packet)
{
}

void CsApexViewCoreRemote::handleBroadcast(BroadcastMessageConstPtr packet)
{
    if(packet) {
        //                std::cout << "type=" << (int) serial->getPacketType() << std::endl;

        switch(packet->getPacketType()) {
        case BroadcastMessage::PACKET_TYPE_ID:
            if(BroadcastMessageConstPtr broadcast = std::dynamic_pointer_cast<BroadcastMessage const>(packet)) {
                if(auto notification_msg = std::dynamic_pointer_cast<NotificationMessage const>(broadcast)) {
                    Notification n = notification_msg->getNotification();
                    notification(n);
                }
            }
        }
    }
}


NodeAdapterFactoryPtr CsApexViewCoreRemote::getNodeAdapterFactory()
{
    return node_adapter_factory_;
}

std::shared_ptr<DragIO> CsApexViewCoreRemote::getDragIO()
{
    return drag_io;
}

/// PROXIES
ExceptionHandler& CsApexViewCoreRemote::getExceptionHandler() const
{
    return *exception_handler_;
}


PluginLocatorPtr CsApexViewCoreRemote::getPluginLocator() const
{
    return core_tmp_->getPluginLocator();
}

CommandExecutorPtr CsApexViewCoreRemote::getCommandDispatcher()
{
    return dispatcher_;
}

Settings& CsApexViewCoreRemote::getSettings() const
{
    return *settings_;
}


GraphFacadePtr CsApexViewCoreRemote::getRoot()
{
    return remote_root_;
}

ThreadPoolPtr CsApexViewCoreRemote::getThreadPool()
{
    // TODO: replace with proxy
    //apex_assert_hard(//core_->getThreadPool());
    return core_tmp_->getThreadPool();
}
NodeFactoryPtr CsApexViewCoreRemote::getNodeFactory() const
{
    // TODO: replace with proxy
    apex_assert_hard(node_factory_);
    return node_factory_;
}
SnippetFactoryPtr CsApexViewCoreRemote::getSnippetFactory() const
{
    // TODO: replace with proxy
    apex_assert_hard(snippet_factory_);
    return snippet_factory_;
}
ProfilerPtr CsApexViewCoreRemote::getProfiler() const
{
    // TODO: replace with proxy
    //apex_assert_hard(//core_->getProfiler() != nullptr);
    return core_tmp_->getProfiler();
}

void CsApexViewCoreRemote::sendNotification(const std::string& notification, ErrorState::ErrorLevel error_level)
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreSendNotification, notification, static_cast<uint8_t>(error_level));
}



/// RELAYS

void CsApexViewCoreRemote::reset()
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreReset);
}


void CsApexViewCoreRemote::load(const std::string& file)
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreLoad, file);
}

void CsApexViewCoreRemote::saveAs(const std::string& file, bool quiet)
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreSave, file, quiet);
}

bool CsApexViewCoreRemote::isPaused() const
{
    auto res = session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreGetPause);
    apex_assert_hard(res);
    return res->getResult<bool>();
}

void CsApexViewCoreRemote::setPause(bool paused)
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreSetPause, paused);
}


bool CsApexViewCoreRemote::isSteppingMode() const
{
    auto res = session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreGetSteppingMode);
    apex_assert_hard(res);
    return res->getResult<bool>();
}

void CsApexViewCoreRemote::setSteppingMode(bool stepping)
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreSetSteppingMode, stepping);
}

void CsApexViewCoreRemote::step()
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreStep);
}


void CsApexViewCoreRemote::shutdown()
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreShutdown);
    running = false;
    io_service.stop();
}

void CsApexViewCoreRemote::clearBlock()
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreClearBlock);
}

void CsApexViewCoreRemote::resetActivity()
{
    session_->sendRequest<CoreRequests>(CoreRequests::CoreRequestType::CoreResetActivity);
}
