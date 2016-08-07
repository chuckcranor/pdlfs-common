/*
 * Copyright (c) 2015-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include "mercury_rpc.h"
#include "pdlfs-common/pdlfs_config.h"
#include "pdlfs-common/testharness.h"
#include "pdlfs-common/testutil.h"

#if defined(MERCURY)
namespace pdlfs {
namespace rpc {

class MercuryServer : public If {
 public:
  MercuryServer() {
    env_ = Env::Default();
    pool_ = ThreadPool::NewFixed(2);
    RPCOptions options;
    options.env = env_;
    options.extra_workers = pool_;
    options.num_io_threads = 2;
    options.uri = "bmi+tcp://10101";
    options.fs = this;
    bool listen = true;
    rpc_ = new MercuryRPC(listen, options);
    looper_ = new MercuryRPC::LocalLooper(rpc_, options);
    self_ = new MercuryRPC::Client(rpc_, "bmi+tcp://localhost:10101");
    looper_->Start();
    rpc_->Ref();
  }

  virtual ~MercuryServer() {
    rpc_->Unref();
    looper_->Stop();
    delete self_;
    delete looper_;
    delete pool_;
  }

  virtual void Call(Message& in, Message& out) {
    out.op = in.op;
    out.err = in.err;
    out.contents = in.contents;
  }

  ThreadPool* pool_;
  MercuryRPC::LocalLooper* looper_;
  MercuryRPC::Client* self_;
  MercuryRPC* rpc_;
  Env* env_;
};

class MercuryTest {
 public:
  MercuryServer* server_;

  MercuryTest() {
    server_ = new MercuryServer();
  }

  ~MercuryTest() {
    delete server_;
  }
};

TEST(MercuryTest, SendReceive) {
  Random rnd(301);
  std::string buf;
  for (int i = 0; i < 1000; ++i) {
    If::Message input;
    If::Message output;
    input.op = rnd.Uniform(128);
    input.err = rnd.Uniform(128);
    input.contents = test::RandomString(&rnd, 4000, &buf);
    server_->self_->Call(input, output);
    ASSERT_EQ(input.op, output.op);
    ASSERT_EQ(input.err, output.err);
    ASSERT_EQ(input.contents, output.contents);
  }
}

}  // namespace rpc
}  // namespace pdlfs

#endif

int main(int argc, char** argv) {
  return ::pdlfs::test::RunAllTests(&argc, &argv);
}
