/// PROJECT
#include <csapex/csapex_window.h>
#include <csapex/box.h>
#include <csapex/boxed_object.h>
#include <csapex/graph.h>

/// SYSTEM
#include <boost/program_options.hpp>
#include <QtGui>
#include <opencv2/opencv.hpp>
#include <QApplication>
#include <signal.h>
#include <X11/Xlib.h>
#include <console_bridge/console.h>

namespace po = boost::program_options;

using namespace csapex;

void siginthandler(int)
{
    printf("User pressed Ctrl+C\n");
    exit(1);
}

namespace csapex
{

struct CsApexApp : public QApplication {
    CsApexApp(int& argc, char** argv)
        : QApplication(argc, argv)
    {}

    virtual bool notify(QObject* receiver, QEvent* event) {
        try {
            return QApplication::notify(receiver, event);

        } catch(const std::exception& e) {
            BoxedObject* bo = dynamic_cast<BoxedObject*> (receiver);
            Box* box = dynamic_cast<Box*> (receiver);
            BoxWorker* bw = dynamic_cast<BoxWorker*> (receiver);

            if(bo) {
                bo->setError(true, e.what());
            } else if(box) {
                box->getContent()->setError(true, e.what());
            } else if(bw) {
                bw->parent()->getContent()->setError(true, e.what());
            } else {
                std::cerr << "Uncatched exception:" << e.what() << std::endl;
            }

            return false;

        } catch(const std::string& s) {
            std::cerr << "Uncatched exception (string) exception: " << s << std::endl;
        } catch(...) {
            std::cerr << "Uncatched exception of unknown type and origin!" << std::endl;
            throw;
        }

        return true;
    }
};

}

int main(int argc, char** argv)
{
//    console_bridge::setLogLevel(console_bridge::CONSOLE_BRIDGE_LOG_DEBUG);
    console_bridge::setLogLevel(console_bridge::CONSOLE_BRIDGE_LOG_INFO);

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "show help message")
            ;

    po::variables_map vm;
    po::notify(vm);

    if(vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    CsApexApp app(argc, argv);

    Graph::Ptr graph(new Graph);
    Graph::setRoot(graph);

    /*
     * There seems to be a bug in Qt4:
     *  A race condition in QApplication sometimes causes a deadlock on startup when using the GTK theme!
     *  Workaround: Specify as a program argument: '-style Plastique' for the Plastique theme or other non-GTK based theme.
     */
    CsApexWindow w;
    w.start();

    app.connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
    signal(SIGINT, siginthandler);
    int result = app.exec();

    return result;
}

