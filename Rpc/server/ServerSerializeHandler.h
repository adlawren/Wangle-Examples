#pragma once

#include <sstream>

#include <wangle/channel/Handler.h>

#include "TestClass.h"

class TestClassSerializer {
public:
  std::string toStdString(TestStruct &ts) {
    std::stringstream ss;
    ss << ts.message << ":" << ts.id;

    return ss.str();
  }

  TestStruct toTestStruct(std::string &s) {
    std::vector<std::string> tokens = split(s, ':');

    TestStruct ts;
    ts.message = tokens[0];
    ts.id = atoi(tokens[1].c_str());

    return ts;
  }
private:
  std::vector<std::string> split(const std::string &s, const char delimiter)
  {
     std::vector<std::string> res;

     if (s.empty())
        return res;

     size_t pos;
     pos = s.find(delimiter);
     if (pos != std::string::npos)
     {
        // found it
        res.push_back(s.substr(0, pos));

        std::vector<std::string> to_add = split(s.substr(pos + 1, s.size()), delimiter);
        res.insert(res.end(), to_add.begin(), to_add.end());
     }
     else
     {
        res.push_back(s);
     }

     return res;
  }
};

class ServerSerializeHandler : public wangle::Handler<
  std::unique_ptr<folly::IOBuf>, TestStruct,
  TestStruct, std::unique_ptr<folly::IOBuf>> {
  public:
    virtual void read(Context *ctx, std::unique_ptr<folly::IOBuf> msg) override {
      std::string received = msg->moveToFbString().toStdString();
      TestStruct ts = ser.toTestStruct(received);

      ctx->fireRead(ts);
    }

    virtual folly::Future<folly::Unit> write(Context *ctx, TestStruct ts) override {
      std::string out = ser.toStdString(ts);

      return ctx->fireWrite(folly::IOBuf::copyBuffer(out));
    }

  private:
    TestClassSerializer ser;
};
