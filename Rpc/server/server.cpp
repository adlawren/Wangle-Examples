#include <iostream>

#include <gflags/gflags.h>

#include <wangle/service/Service.h>
#include <wangle/service/ExpiringFilter.h>
#include <wangle/service/ExecutorFilter.h>
#include <wangle/service/ServerDispatcher.h>
#include <wangle/bootstrap/ServerBootstrap.h>
#include <wangle/channel/AsyncSocketHandler.h>
#include <wangle/codec/LengthFieldBasedFrameDecoder.h>
#include <wangle/codec/LengthFieldPrepender.h>
#include <wangle/channel/EventBaseHandler.h>
#include <wangle/concurrent/CPUThreadPoolExecutor.h>

#include "TestClass.h"
#include "ServerSerializeHandler.h"

using namespace folly;
using namespace wangle;

DEFINE_int32(port, 8080, "echo server port");

typedef Pipeline<IOBufQueue&, std::string> EchoPipeline;

class TestService : public Service<TestStruct, TestStruct> {
public:
  virtual Future<TestStruct> operator()(TestStruct request) override {
    std::cout << "Recieved: " << request.message  << ", with id: " << request.id << std::endl;

    return futures::sleep(std::chrono::seconds(request.id))
      .then([request]() {
        TestStruct response;
        response.message = request.message + "asdf";
        response.id = request.id;

        return response;
      });
  }
};

// where we define the chain of handlers for each messeage received
class EchoPipelineFactory : public PipelineFactory<EchoPipeline> {
public:
  EchoPipeline::Ptr newPipeline(std::shared_ptr<AsyncTransportWrapper> sock) {
    auto pipeline = EchoPipeline::create();
    pipeline->addBack(AsyncSocketHandler(sock));
    pipeline->addBack(EventBaseHandler());
    pipeline->addBack(LengthFieldBasedFrameDecoder());
    pipeline->addBack(LengthFieldPrepender());
    pipeline->addBack(ServerSerializeHandler());
    pipeline->addBack(MultiplexServerDispatcher<TestStruct, TestStruct>(&service_));
    pipeline->finalize();
    return pipeline;
  }
private:
  ExecutorFilter<TestStruct, TestStruct> service_{
    std::make_shared<CPUThreadPoolExecutor>(10),
    std::make_shared<TestService>()
  };
};

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  ServerBootstrap<EchoPipeline> server;
  server.childPipeline(std::make_shared<EchoPipelineFactory>());
  server.bind(FLAGS_port);
  server.waitForStop();

  return 0;
}
