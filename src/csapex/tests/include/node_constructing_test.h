#ifndef NODE_CONSTRUCTING_TEST_H
#define NODE_CONSTRUCTING_TEST_H

#include <csapex/model/model_fwd.h>
#include <csapex/factory/node_factory.h>
#include <csapex/factory/node_wrapper.hpp>
#include <csapex/core/settings/settings_local.h>
#include <csapex/scheduling/thread_pool.h>
#include <csapex/msg/output.h>
#include <csapex/msg/input.h>
#include <csapex/msg/io.h>
#include <csapex/model/subgraph_node.h>
#include <csapex/model/graph.h>
#include <csapex/model/graph/vertex.h>
#include <csapex/model/node_facade_local.h>

#include "gtest/gtest.h"

#include "test_exception_handler.h"
#include "mockup_nodes.h"

namespace csapex
{

class NodeConstructingTest : public ::testing::Test {
protected:
    NodeConstructingTest()
        : factory(SettingsLocal::NoSettings, nullptr),
          executor(eh, false, false)
    {
        factory.registerNodeType(std::make_shared<NodeConstructor>("StaticMultiplier", std::bind(&NodeConstructingTest::makeStaticMultiplier<2>)));
        factory.registerNodeType(std::make_shared<NodeConstructor>("StaticMultiplier4", std::bind(&NodeConstructingTest::makeStaticMultiplier<4>)));
        factory.registerNodeType(std::make_shared<NodeConstructor>("StaticMultiplier7", std::bind(&NodeConstructingTest::makeStaticMultiplier<7>)));
        factory.registerNodeType(std::make_shared<NodeConstructor>("DynamicMultiplier", std::bind(&NodeConstructingTest::makeDynamicMultiplier)));
        factory.registerNodeType(std::make_shared<NodeConstructor>("MockupSource", std::bind(&NodeConstructingTest::makeSource)));
        factory.registerNodeType(std::make_shared<NodeConstructor>("MockupSink", std::bind(&NodeConstructingTest::makeSink)));
    }

    virtual ~NodeConstructingTest() {
    }

    virtual void SetUp() override {
        graph_node = std::make_shared<SubgraphNode>(std::make_shared<GraphLocal>());
        graph = graph_node->getGraph();

        abort_connection = error_handling::stop_request().connect([this](){
            for(auto it = graph->begin(); it != graph->end(); ++it) {
                NodeFacadePtr nf = (*it)->getNodeFacade();
                if(std::shared_ptr<MockupSink> mp = std::dynamic_pointer_cast<MockupSink>(nf->getNode())) {
                    mp->abort();
                }
            }
        });
    }

    virtual void TearDown() override {
        abort_connection.disconnect();
    }

    template <int factor>
    static NodePtr makeStaticMultiplier() {
        return NodePtr(new NodeWrapper<MockupStaticMultiplierNode<factor>>());
    }
    static NodePtr makeDynamicMultiplier() {
        return NodePtr(new NodeWrapper<MockupDynamicMultiplierNode>());
    }
    static NodePtr makeSource() {
        return NodePtr(new MockupSource());
    }
    static NodePtr makeSink() {
        return NodePtr(new NodeWrapper<MockupSink>());
    }

    NodeFactory factory;
    TestExceptionHandler eh;

    ThreadPool executor;

    SubgraphNodePtr graph_node;
    GraphPtr graph;

    slim_signal::Connection abort_connection;
};
}

#endif // NODE_CONSTRUCTING_TEST_H
