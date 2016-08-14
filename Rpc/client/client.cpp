#include <gflags/gflags.h>
#include <iostream>

#include <wangle/service/Service.h>
#include <wangle/service/ExpiringFilter.h>
#include <wangle/service/ClientDispatcher.h>
#include <wangle/bootstrap/ClientBootstrap.h>
#include <wangle/channel/AsyncSocketHandler.h>
#include <wangle/channel/EventBaseHandler.h>
#include <wangle/codec/LengthFieldBasedFrameDecoder.h>
#include <wangle/codec/LengthFieldPrepender.h>

#include "TestClass.h"
#include "ClientSerializeHandler.h"

using namespace folly;
using namespace wangle;

DEFINE_int32(port, 8080, "echo server port");
DEFINE_string(host, "::1", "echo server address");

typedef Pipeline<folly::IOBufQueue&, TestStruct> EchoPipeline;

class EchoPipelineFactory : public PipelineFactory<EchoPipeline> {
public:
  EchoPipeline::Ptr newPipeline(std::shared_ptr<AsyncTransportWrapper> sock) {
    auto pipeline = EchoPipeline::create();
    pipeline->addBack(AsyncSocketHandler(sock));
    pipeline->addBack(EventBaseHandler());
    pipeline->addBack(LengthFieldBasedFrameDecoder());
    pipeline->addBack(LengthFieldPrepender());
    pipeline->addBack(ClientSerializeHandler());
    pipeline->finalize();
    return pipeline;
  }
};

class TestClassMultiplexClientDispatcher
    : public ClientDispatcherBase<EchoPipeline, TestStruct, TestStruct> {
  public:
    void read(Context *ctx, TestStruct in) override {
      auto search = requests_.find(in.id);
      CHECK(search != requests_.end());
      auto p = std::move(search->second);
      requests_.erase(in.id);
      p.setValue(in);
    }

    Future<TestStruct> operator()(TestStruct arg) override {
      auto& p = requests_[arg.id];
      auto f = p.getFuture();
      p.setInterruptHandler([arg, this](const folly::exception_wrapper& e) {
        this->requests_.erase(arg.id);
      });

      this->pipeline_->write(arg);

      return f;
    }

  virtual Future<Unit> close() override {
    std::cout << "Channel closed!" << std::endl;

    return ClientDispatcherBase::close();
  }

  virtual Future<Unit> close(Context *ctx) override {
    std::cout << "Channel closed!" << std::endl;

    return ClientDispatcherBase::close(ctx);
  }

  private:
    std::unordered_map<int32_t, Promise<TestStruct>> requests_;
};

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  ClientBootstrap<EchoPipeline> client;
  client.group(std::make_shared<wangle::IOThreadPoolExecutor>(1));
  client.pipelineFactory(std::make_shared<EchoPipelineFactory>());
  auto pipeline = client.connect(SocketAddress(FLAGS_host, FLAGS_port)).get();

  auto dispatcher = std::make_shared<TestClassMultiplexClientDispatcher>();
  dispatcher->setPipeline(pipeline);

  ExpiringFilter<TestStruct, TestStruct> service(dispatcher, std::chrono::seconds(5));

  try {
    while (true) {
      std::cout << "Provide a string and a number, separated by a space:" << std::endl;

      TestStruct request;
      std::cin >> request.message;
      std::cin >> request.id;

      service(request).then([request](TestStruct response) {
        CHECK(request.id == response.id);
        std::cout << "Response: " << response.message << std::endl;
      });
    }
  } catch (const std::exception& e) {
    std::cout << exceptionStr(e) << std::endl;
  }

  return 0;
}
