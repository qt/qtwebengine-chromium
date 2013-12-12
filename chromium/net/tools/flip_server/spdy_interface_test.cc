// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/flip_server/spdy_interface.h"

#include <list>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/tools/flip_server/balsa_enums.h"
#include "net/tools/flip_server/balsa_headers.h"
#include "net/tools/flip_server/flip_config.h"
#include "net/tools/flip_server/flip_test_utils.h"
#include "net/tools/flip_server/mem_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using ::base::StringPiece;
using ::testing::_;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;

namespace {

struct StringSaver {
 public:
  StringSaver() : data(NULL), size(0) {}
  void Save() {
    string = std::string(data, size);
  }

  const char* data;
  size_t size;
  std::string string;
};

class SpdyFramerVisitor : public BufferedSpdyFramerVisitorInterface {
 public:
  virtual ~SpdyFramerVisitor() {}
  MOCK_METHOD1(OnError, void(SpdyFramer::SpdyError));
  MOCK_METHOD2(OnStreamError, void(SpdyStreamId, const std::string&));
  MOCK_METHOD7(OnSynStream, void(SpdyStreamId,
                                 SpdyStreamId,
                                 SpdyPriority,
                                 uint8,
                                 bool,
                                 bool,
                                 const SpdyHeaderBlock&));
  MOCK_METHOD3(OnSynStream, void(SpdyStreamId, bool, const SpdyHeaderBlock&));
  MOCK_METHOD3(OnSynReply, void(SpdyStreamId, bool, const SpdyHeaderBlock&));
  MOCK_METHOD3(OnHeaders, void(SpdyStreamId, bool, const SpdyHeaderBlock&));
  MOCK_METHOD4(OnStreamFrameData, void(SpdyStreamId,
                                       const char*,
                                       size_t,
                                       bool));
  MOCK_METHOD1(OnStreamFrameData, void(bool));
  MOCK_METHOD1(OnSettings, void(bool));
  MOCK_METHOD3(OnSetting, void(SpdySettingsIds, uint8, uint32));
  MOCK_METHOD1(OnPing, void(uint32));
  MOCK_METHOD2(OnRstStream, void(SpdyStreamId, SpdyRstStreamStatus));
  MOCK_METHOD2(OnGoAway, void(SpdyStreamId, SpdyGoAwayStatus));
  MOCK_METHOD2(OnWindowUpdate, void(SpdyStreamId, uint32));
  MOCK_METHOD2(OnPushPromise, void(SpdyStreamId, SpdyStreamId));
};

class FakeSMConnection : public SMConnection {
 public:
  FakeSMConnection(EpollServer* epoll_server,
                   SSLState* ssl_state,
                   MemoryCache* memory_cache,
                   FlipAcceptor* acceptor,
                   std::string log_prefix)
      : SMConnection(epoll_server,
                     ssl_state,
                     memory_cache,
                     acceptor,
                     log_prefix) {}

  MOCK_METHOD0(Cleanup, void());
  MOCK_METHOD8(InitSMConnection, void(SMConnectionPoolInterface*,
                                      SMInterface*,
                                      EpollServer*,
                                      int,
                                      std::string,
                                      std::string,
                                      std::string,
                                      bool));
};

class SpdySMWithMockSMInterfaceFactory : public SpdySM {
 public:
  virtual ~SpdySMWithMockSMInterfaceFactory() {}
  SpdySMWithMockSMInterfaceFactory(SMConnection* connection,
                                   SMInterface* sm_http_interface,
                                   EpollServer* epoll_server,
                                   MemoryCache* memory_cache,
                                   FlipAcceptor* acceptor)
      : SpdySM(connection,
               sm_http_interface,
               epoll_server,
               memory_cache,
               acceptor) {}

  MOCK_METHOD2(FindOrMakeNewSMConnectionInterface,
               SMInterface*(const std::string&, const std::string&));
};

class FlipSpdySMTest : public ::testing::Test {
 public:
  FlipSpdySMTest(SpdyMajorVersion version = SPDY2,
                 FlipHandlerType type = FLIP_HANDLER_PROXY)
      : spdy_version_(version) {
    SSLState* ssl_state = NULL;
    mock_another_interface_.reset(new MockSMInterface);
    memory_cache_.reset(new MemoryCache);
    acceptor_.reset(new FlipAcceptor(type,
                                     "127.0.0.1",
                                     "8941",
                                     "ssl_cert_filename",
                                     "ssl_key_filename",
                                     "127.0.0.1",
                                     "8942",
                                     "127.0.0.1",
                                     "8943",
                                     1,
                                     0,
                                     true,
                                     1,
                                     false,
                                     true,
                                     NULL));
    epoll_server_.reset(new EpollServer);
    connection_.reset(new FakeSMConnection(epoll_server_.get(),
                                           ssl_state,
                                           memory_cache_.get(),
                                           acceptor_.get(),
                                           "log_prefix"));

    interface_.reset(new SpdySMWithMockSMInterfaceFactory(
        connection_.get(),
        mock_another_interface_.get(),
        epoll_server_.get(),
        memory_cache_.get(),
        acceptor_.get()));

    spdy_framer_.reset(new BufferedSpdyFramer(spdy_version_, true));
    spdy_framer_visitor_.reset(new SpdyFramerVisitor);
    spdy_framer_->set_visitor(spdy_framer_visitor_.get());
  }

  virtual void TearDown() OVERRIDE {
    if (acceptor_->listen_fd_ >= 0) {
      epoll_server_->UnregisterFD(acceptor_->listen_fd_);
      close(acceptor_->listen_fd_);
      acceptor_->listen_fd_ = -1;
    }
    OutputList& output_list = *connection_->output_list();
    for (OutputList::const_iterator i = output_list.begin();
         i != output_list.end();
         ++i) {
      delete *i;
    }
    output_list.clear();
  }

  bool HasStream(uint32 stream_id) {
    return interface_->output_ordering().ExistsInPriorityMaps(stream_id);
  }

 protected:
  SpdyMajorVersion spdy_version_;
  scoped_ptr<MockSMInterface> mock_another_interface_;
  scoped_ptr<MemoryCache> memory_cache_;
  scoped_ptr<FlipAcceptor> acceptor_;
  scoped_ptr<EpollServer> epoll_server_;
  scoped_ptr<FakeSMConnection> connection_;
  scoped_ptr<SpdySMWithMockSMInterfaceFactory> interface_;
  scoped_ptr<BufferedSpdyFramer> spdy_framer_;
  scoped_ptr<SpdyFramerVisitor> spdy_framer_visitor_;
};

class FlipSpdy2SMTest : public FlipSpdySMTest {
 public:
  FlipSpdy2SMTest(): FlipSpdySMTest(SPDY2) {}
  virtual ~FlipSpdy2SMTest() {}
};

class FlipSpdy2SMTestNonProxy : public FlipSpdySMTest {
 public:
  FlipSpdy2SMTestNonProxy(): FlipSpdySMTest(SPDY2, FLIP_HANDLER_SPDY_SERVER) {}
  virtual ~FlipSpdy2SMTestNonProxy() {}
};

TEST_F(FlipSpdySMTest, InitSMConnection) {
  {
    InSequence s;
    EXPECT_CALL(*connection_, InitSMConnection(_, _, _, _, _, _, _, _));
  }
  interface_->InitSMConnection(NULL,
                               NULL,
                               epoll_server_.get(),
                               -1,
                               "",
                               "",
                               "",
                               false);
}

TEST_F(FlipSpdySMTest, OnSynStream) {
  BufferedSpdyFramerVisitorInterface* visitor = interface_.get();
  scoped_ptr<MockSMInterface> mock_interface(new MockSMInterface);
  uint32 stream_id = 92;
  uint32 associated_id = 43;
  std::string expected = "GET /path HTTP/1.0\r\n"
      "method: GET\r\n"
      "scheme: http\r\n"
      "url: http://www.example.com/path\r\n"
      "version: HTTP/1.0\r\n\r\n";
  SpdyHeaderBlock block;
  block["method"] = "GET";
  block["url"] = "http://www.example.com/path";
  block["scheme"] = "http";
  block["version"] = "HTTP/1.0";
  StringSaver saver;
  {
    InSequence s;
    EXPECT_CALL(*interface_,
                FindOrMakeNewSMConnectionInterface(_, _))
        .WillOnce(Return(mock_interface.get()));
    EXPECT_CALL(*mock_interface, SetStreamID(stream_id));
    EXPECT_CALL(*mock_interface, ProcessWriteInput(_, _))
        .WillOnce(DoAll(SaveArg<0>(&saver.data),
                        SaveArg<1>(&saver.size),
                        InvokeWithoutArgs(&saver, &StringSaver::Save),
                        Return(0)));
  }
  visitor->OnSynStream(stream_id, associated_id, 0, 0, false, false, block);
  ASSERT_EQ(expected, saver.string);
}

TEST_F(FlipSpdySMTest, OnStreamFrameData) {
  BufferedSpdyFramerVisitorInterface* visitor = interface_.get();
  scoped_ptr<MockSMInterface> mock_interface(new MockSMInterface);
  uint32 stream_id = 92;
  uint32 associated_id = 43;
  SpdyHeaderBlock block;
  testing::MockFunction<void(int)> checkpoint;

  scoped_ptr<SpdyFrame> frame(spdy_framer_->CreatePingFrame(12));
  block["method"] = "GET";
  block["url"] = "http://www.example.com/path";
  block["scheme"] = "http";
  block["version"] = "HTTP/1.0";
  {
    InSequence s;
    EXPECT_CALL(*interface_,
                FindOrMakeNewSMConnectionInterface(_, _))
        .WillOnce(Return(mock_interface.get()));
    EXPECT_CALL(*mock_interface, SetStreamID(stream_id));
    EXPECT_CALL(*mock_interface, ProcessWriteInput(_, _)).Times(1);
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*mock_interface,
                ProcessWriteInput(frame->data(), frame->size())).Times(1);
  }

  visitor->OnSynStream(stream_id, associated_id, 0, 0, false, false, block);
  checkpoint.Call(0);
  visitor->OnStreamFrameData(stream_id, frame->data(), frame->size(), true);
}

TEST_F(FlipSpdySMTest, OnRstStream) {
  BufferedSpdyFramerVisitorInterface* visitor = interface_.get();
  uint32 stream_id = 82;
  MemCacheIter mci;
  mci.stream_id = stream_id;

  {
    BalsaHeaders headers;
    std::string filename = "foobar";
    memory_cache_->InsertFile(&headers, filename, "");
    mci.file_data = memory_cache_->GetFileData(filename);
  }

  interface_->AddToOutputOrder(mci);
  ASSERT_TRUE(HasStream(stream_id));
  visitor->OnRstStream(stream_id, RST_STREAM_INVALID);
  ASSERT_FALSE(HasStream(stream_id));
}

TEST_F(FlipSpdySMTest, ProcessReadInput) {
  ASSERT_EQ(SpdyFramer::SPDY_RESET, interface_->spdy_framer()->state());
  interface_->ProcessReadInput("", 1);
  ASSERT_EQ(SpdyFramer::SPDY_READING_COMMON_HEADER,
            interface_->spdy_framer()->state());
}

TEST_F(FlipSpdySMTest, ResetForNewConnection) {
  uint32 stream_id = 13;
  MemCacheIter mci;
  mci.stream_id = stream_id;
  // incomplete input
  const char input[] = {'\0', '\0', '\0'};

  {
    BalsaHeaders headers;
    std::string filename = "foobar";
    memory_cache_->InsertFile(&headers, filename, "");
    mci.file_data = memory_cache_->GetFileData(filename);
  }

  interface_->AddToOutputOrder(mci);
  ASSERT_TRUE(HasStream(stream_id));
  interface_->ProcessReadInput(input, sizeof(input));
  ASSERT_NE(SpdyFramer::SPDY_RESET, interface_->spdy_framer()->state());

  interface_->ResetForNewConnection();
  ASSERT_FALSE(HasStream(stream_id));
  ASSERT_EQ(SpdyFramer::SPDY_RESET, interface_->spdy_framer()->state());
}

TEST_F(FlipSpdySMTest, PostAcceptHook) {
  interface_->PostAcceptHook();

  ASSERT_EQ(1u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;

  {
    InSequence s;
    EXPECT_CALL(*spdy_framer_visitor_, OnSettings(false));
    EXPECT_CALL(*spdy_framer_visitor_, OnSetting(
        SETTINGS_MAX_CONCURRENT_STREAMS, 0u, 100u));
  }
  spdy_framer_->ProcessInput(df->data, df->size);
}

TEST_F(FlipSpdySMTest, NewStream) {
  // TODO(yhirano): SpdySM::NewStream leads to crash when
  // acceptor_->flip_handler_type_ != FLIP_HANDLER_SPDY_SERVER.
  // It should be fixed though I don't know the solution now.
}

TEST_F(FlipSpdySMTest, AddToOutputOrder) {
  uint32 stream_id = 13;
  MemCacheIter mci;
  mci.stream_id = stream_id;

  {
    BalsaHeaders headers;
    std::string filename = "foobar";
    memory_cache_->InsertFile(&headers, filename, "");
    mci.file_data = memory_cache_->GetFileData(filename);
  }

  interface_->AddToOutputOrder(mci);
  ASSERT_TRUE(HasStream(stream_id));
}

TEST_F(FlipSpdySMTest, SendErrorNotFound) {
  uint32 stream_id = 82;
  SpdyHeaderBlock actual_header_block;
  const char* actual_data;
  size_t actual_size;
  testing::MockFunction<void(int)> checkpoint;

  interface_->SendErrorNotFound(stream_id);

  ASSERT_EQ(2u, connection_->output_list()->size());

  {
    InSequence s;
    EXPECT_CALL(*spdy_framer_visitor_,
                OnSynReply(stream_id, false, _))
        .WillOnce(SaveArg<2>(&actual_header_block));
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*spdy_framer_visitor_,
                OnStreamFrameData(stream_id, _, _, false)).Times(1)
        .WillOnce(DoAll(SaveArg<1>(&actual_data),
                        SaveArg<2>(&actual_size)));
    EXPECT_CALL(*spdy_framer_visitor_,
                OnStreamFrameData(stream_id, NULL, 0, true)).Times(1);
  }

  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;
  spdy_framer_->ProcessInput(df->data, df->size);
  checkpoint.Call(0);
  df = *i++;
  spdy_framer_->ProcessInput(df->data, df->size);

  ASSERT_EQ(2, spdy_framer_->frames_received());
  ASSERT_EQ(2u, actual_header_block.size());
  ASSERT_EQ("404 Not Found", actual_header_block["status"]);
  ASSERT_EQ("HTTP/1.1", actual_header_block["version"]);
  ASSERT_EQ("wtf?", StringPiece(actual_data, actual_size));
}

TEST_F(FlipSpdySMTest, SendSynStream) {
  uint32 stream_id = 82;
  BalsaHeaders headers;
  SpdyHeaderBlock actual_header_block;
  headers.AppendHeader("key1", "value1");
  headers.SetRequestFirstlineFromStringPieces("GET", "/path", "HTTP/1.0");

  interface_->SendSynStream(stream_id, headers);

  ASSERT_EQ(1u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;

  {
    InSequence s;
    EXPECT_CALL(*spdy_framer_visitor_,
                OnSynStream(stream_id, 0, _, _, false, false, _))
        .WillOnce(SaveArg<6>(&actual_header_block));
  }

  spdy_framer_->ProcessInput(df->data, df->size);
  ASSERT_EQ(1, spdy_framer_->frames_received());
  ASSERT_EQ(4u, actual_header_block.size());
  ASSERT_EQ("GET", actual_header_block["method"]);
  ASSERT_EQ("HTTP/1.0", actual_header_block["version"]);
  ASSERT_EQ("/path", actual_header_block["url"]);
  ASSERT_EQ("value1", actual_header_block["key1"]);
}

TEST_F(FlipSpdySMTest, SendSynReply) {
  uint32 stream_id = 82;
  BalsaHeaders headers;
  SpdyHeaderBlock actual_header_block;
  headers.AppendHeader("key1", "value1");
  headers.SetResponseFirstlineFromStringPieces("HTTP/1.1", "200", "OK");

  interface_->SendSynReply(stream_id, headers);

  ASSERT_EQ(1u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;

  {
    InSequence s;
    EXPECT_CALL(*spdy_framer_visitor_, OnSynReply(stream_id, false, _))
        .WillOnce(SaveArg<2>(&actual_header_block));
  }

  spdy_framer_->ProcessInput(df->data, df->size);
  ASSERT_EQ(1, spdy_framer_->frames_received());
  ASSERT_EQ(3u, actual_header_block.size());
  ASSERT_EQ("200 OK", actual_header_block["status"]);
  ASSERT_EQ("HTTP/1.1", actual_header_block["version"]);
  ASSERT_EQ("value1", actual_header_block["key1"]);
}

TEST_F(FlipSpdySMTest, SendDataFrame) {
  uint32 stream_id = 133;
  SpdyDataFlags flags = DATA_FLAG_NONE;
  const char* actual_data;
  size_t actual_size;

  interface_->SendDataFrame(stream_id, "hello", 5, flags, true);

  ASSERT_EQ(1u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;

  {
    InSequence s;
    EXPECT_CALL(*spdy_framer_visitor_,
                OnStreamFrameData(stream_id, _, _, false))
        .WillOnce(DoAll(SaveArg<1>(&actual_data),
                        SaveArg<2>(&actual_size)));
  }

  spdy_framer_->ProcessInput(df->data, df->size);
  ASSERT_EQ(1, spdy_framer_->frames_received());
  ASSERT_EQ("hello", StringPiece(actual_data, actual_size));
}

TEST_F(FlipSpdySMTest, SendLongDataFrame) {
  uint32 stream_id = 133;
  SpdyDataFlags flags = DATA_FLAG_NONE;
  const char* actual_data;
  size_t actual_size;

  std::string data =
      std::string(kSpdySegmentSize, 'a') +
      std::string(kSpdySegmentSize, 'b') +
      "c";
  interface_->SendDataFrame(stream_id, data.data(), data.size(), flags, true);

  {
    InSequence s;
    EXPECT_CALL(*spdy_framer_visitor_,
                OnStreamFrameData(stream_id, _, _, false)).Times(3)
        .WillRepeatedly(DoAll(SaveArg<1>(&actual_data),
                        SaveArg<2>(&actual_size)));
  }

  ASSERT_EQ(3u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;
  spdy_framer_->ProcessInput(df->data, df->size);
  ASSERT_EQ(std::string(kSpdySegmentSize, 'a'),
            StringPiece(actual_data, actual_size));

  df = *i++;
  spdy_framer_->ProcessInput(df->data, df->size);
  ASSERT_EQ(std::string(kSpdySegmentSize, 'b'),
            StringPiece(actual_data, actual_size));

  df = *i++;
  spdy_framer_->ProcessInput(df->data, df->size);
  ASSERT_EQ("c", StringPiece(actual_data, actual_size));
}

TEST_F(FlipSpdy2SMTest, SendEOF) {
  uint32 stream_id = 82;
  // SPDY2 data frame
  char empty_data_frame[] = {'\0', '\0', '\0', '\x52', '\x1', '\0', '\0', '\0'};
  MemCacheIter mci;
  mci.stream_id = stream_id;

  {
    BalsaHeaders headers;
    std::string filename = "foobar";
    memory_cache_->InsertFile(&headers, filename, "");
    mci.file_data = memory_cache_->GetFileData(filename);
  }

  interface_->AddToOutputOrder(mci);
  ASSERT_TRUE(HasStream(stream_id));
  interface_->SendEOF(stream_id);
  ASSERT_FALSE(HasStream(stream_id));

  ASSERT_EQ(1u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;
  ASSERT_EQ(StringPiece(empty_data_frame, sizeof(empty_data_frame)),
            StringPiece(df->data, df->size));
}

TEST_F(FlipSpdy2SMTest, SendEmptyDataFrame) {
  uint32 stream_id = 133;
  SpdyDataFlags flags = DATA_FLAG_NONE;
  // SPDY2 data frame
  char expected[] = {'\0', '\0', '\0', '\x85', '\0', '\0', '\0', '\0'};

  interface_->SendDataFrame(stream_id, "hello", 0, flags, true);

  ASSERT_EQ(1u, connection_->output_list()->size());
  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;

  ASSERT_EQ(StringPiece(expected, sizeof(expected)),
            StringPiece(df->data, df->size));
}

TEST_F(FlipSpdy2SMTestNonProxy, OnSynStream) {
  BufferedSpdyFramerVisitorInterface* visitor = interface_.get();
  uint32 stream_id = 82;
  SpdyHeaderBlock spdy_headers;
  spdy_headers["url"] = "http://www.example.com/path";
  spdy_headers["method"] = "GET";
  spdy_headers["scheme"] = "http";

  {
    BalsaHeaders headers;
    memory_cache_->InsertFile(&headers, "GET_/path", "");
  }
  visitor->OnSynStream(stream_id, 0, 0, 0, true, true, spdy_headers);
  ASSERT_TRUE(HasStream(stream_id));
}

TEST_F(FlipSpdy2SMTestNonProxy, NewStream) {
  uint32 stream_id = 13;
  std::string filename = "foobar";

  {
    BalsaHeaders headers;
    memory_cache_->InsertFile(&headers, filename, "");
  }

  interface_->NewStream(stream_id, 0, filename);
  ASSERT_TRUE(HasStream(stream_id));
}

TEST_F(FlipSpdy2SMTestNonProxy, NewStreamError) {
  uint32 stream_id = 82;
  SpdyHeaderBlock actual_header_block;
  const char* actual_data;
  size_t actual_size;
  testing::MockFunction<void(int)> checkpoint;

  interface_->NewStream(stream_id, 0, "nonexistingfile");

  ASSERT_EQ(2u, connection_->output_list()->size());

  {
    InSequence s;
    EXPECT_CALL(*spdy_framer_visitor_,
                OnSynReply(stream_id, false, _))
        .WillOnce(SaveArg<2>(&actual_header_block));
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(*spdy_framer_visitor_,
                OnStreamFrameData(stream_id, _, _, false)).Times(1)
        .WillOnce(DoAll(SaveArg<1>(&actual_data),
                        SaveArg<2>(&actual_size)));
    EXPECT_CALL(*spdy_framer_visitor_,
                OnStreamFrameData(stream_id, NULL, 0, true)).Times(1);
  }

  std::list<DataFrame*>::const_iterator i = connection_->output_list()->begin();
  DataFrame* df = *i++;
  spdy_framer_->ProcessInput(df->data, df->size);
  checkpoint.Call(0);
  df = *i++;
  spdy_framer_->ProcessInput(df->data, df->size);

  ASSERT_EQ(2, spdy_framer_->frames_received());
  ASSERT_EQ(2u, actual_header_block.size());
  ASSERT_EQ("404 Not Found", actual_header_block["status"]);
  ASSERT_EQ("HTTP/1.1", actual_header_block["version"]);
  ASSERT_EQ("wtf?", StringPiece(actual_data, actual_size));
}

}  // namespace

}  // namespace net
