// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_data_stream.h"

#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_spdy_compressor.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/quic/quic_utils.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::StringPiece;
using std::min;
using testing::_;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::StrEq;
using testing::StrictMock;

namespace net {
namespace test {
namespace {

const QuicGuid kStreamId = 3;
const bool kIsServer = true;
const bool kShouldProcessData = true;

class TestStream : public QuicDataStream {
 public:
  TestStream(QuicStreamId id,
             QuicSession* session,
             bool should_process_data)
      : QuicDataStream(id, session),
        should_process_data_(should_process_data) {}

  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE {
    EXPECT_NE(0u, data_len);
    DVLOG(1) << "ProcessData data_len: " << data_len;
    data_ += string(data, data_len);
    return should_process_data_ ? data_len : 0;
  }

  using ReliableQuicStream::WriteOrBufferData;
  using ReliableQuicStream::CloseReadSide;
  using ReliableQuicStream::CloseWriteSide;

  const string& data() const { return data_; }

 private:
  bool should_process_data_;
  string data_;
};

class QuicDataStreamTest : public ::testing::TestWithParam<bool> {
 public:
  QuicDataStreamTest() {
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "https";
    headers_["cookie"] =
        "__utma=208381060.1228362404.1372200928.1372200928.1372200928.1; "
        "__utmc=160408618; "
        "GX=DQAAAOEAAACWJYdewdE9rIrW6qw3PtVi2-d729qaa-74KqOsM1NVQblK4VhX"
        "hoALMsy6HOdDad2Sz0flUByv7etmo3mLMidGrBoljqO9hSVA40SLqpG_iuKKSHX"
        "RW3Np4bq0F0SDGDNsW0DSmTS9ufMRrlpARJDS7qAI6M3bghqJp4eABKZiRqebHT"
        "pMU-RXvTI5D5oCF1vYxYofH_l1Kviuiy3oQ1kS1enqWgbhJ2t61_SNdv-1XJIS0"
        "O3YeHLmVCs62O6zp89QwakfAWK9d3IDQvVSJzCQsvxvNIvaZFa567MawWlXg0Rh"
        "1zFMi5vzcns38-8_Sns; "
        "GA=v*2%2Fmem*57968640*47239936%2Fmem*57968640*47114716%2Fno-nm-"
        "yj*15%2Fno-cc-yj*5%2Fpc-ch*133685%2Fpc-s-cr*133947%2Fpc-s-t*1339"
        "47%2Fno-nm-yj*4%2Fno-cc-yj*1%2Fceft-as*1%2Fceft-nqas*0%2Fad-ra-c"
        "v_p%2Fad-nr-cv_p-f*1%2Fad-v-cv_p*859%2Fad-ns-cv_p-f*1%2Ffn-v-ad%"
        "2Fpc-t*250%2Fpc-cm*461%2Fpc-s-cr*722%2Fpc-s-t*722%2Fau_p*4"
        "SICAID=AJKiYcHdKgxum7KMXG0ei2t1-W4OD1uW-ecNsCqC0wDuAXiDGIcT_HA2o1"
        "3Rs1UKCuBAF9g8rWNOFbxt8PSNSHFuIhOo2t6bJAVpCsMU5Laa6lewuTMYI8MzdQP"
        "ARHKyW-koxuhMZHUnGBJAM1gJODe0cATO_KGoX4pbbFxxJ5IicRxOrWK_5rU3cdy6"
        "edlR9FsEdH6iujMcHkbE5l18ehJDwTWmBKBzVD87naobhMMrF6VvnDGxQVGp9Ir_b"
        "Rgj3RWUoPumQVCxtSOBdX0GlJOEcDTNCzQIm9BSfetog_eP_TfYubKudt5eMsXmN6"
        "QnyXHeGeK2UINUzJ-D30AFcpqYgH9_1BvYSpi7fc7_ydBU8TaD8ZRxvtnzXqj0RfG"
        "tuHghmv3aD-uzSYJ75XDdzKdizZ86IG6Fbn1XFhYZM-fbHhm3mVEXnyRW4ZuNOLFk"
        "Fas6LMcVC6Q8QLlHYbXBpdNFuGbuZGUnav5C-2I_-46lL0NGg3GewxGKGHvHEfoyn"
        "EFFlEYHsBQ98rXImL8ySDycdLEFvBPdtctPmWCfTxwmoSMLHU2SCVDhbqMWU5b0yr"
        "JBCScs_ejbKaqBDoB7ZGxTvqlrB__2ZmnHHjCr8RgMRtKNtIeuZAo ";
  }

  void Initialize(bool stream_should_process_data) {
    connection_ = new StrictMock<MockConnection>(kIsServer);
    session_.reset(new StrictMock<MockSession>(connection_));
    stream_.reset(new TestStream(kStreamId, session_.get(),
                                 stream_should_process_data));
    stream2_.reset(new TestStream(kStreamId + 2, session_.get(),
                                 stream_should_process_data));
    compressor_.reset(new QuicSpdyCompressor());
    decompressor_.reset(new QuicSpdyDecompressor);
    write_blocked_list_ =
        QuicSessionPeer::GetWriteblockedStreams(session_.get());
  }

 protected:
  MockConnection* connection_;
  scoped_ptr<MockSession> session_;
  scoped_ptr<TestStream> stream_;
  scoped_ptr<TestStream> stream2_;
  scoped_ptr<QuicSpdyCompressor> compressor_;
  scoped_ptr<QuicSpdyDecompressor> decompressor_;
  SpdyHeaderBlock headers_;
  WriteBlockedList<QuicStreamId>* write_blocked_list_;
};

TEST_F(QuicDataStreamTest, ProcessHeaders) {
  Initialize(kShouldProcessData);

  string compressed_headers = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  QuicStreamFrame frame(kStreamId, false, 0, MakeIOVector(compressed_headers));

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers_), stream_->data());
  EXPECT_EQ(QuicUtils::HighestPriority(), stream_->EffectivePriority());
}

TEST_F(QuicDataStreamTest, ProcessHeadersWithInvalidHeaderId) {
  Initialize(kShouldProcessData);

  string compressed_headers = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  compressed_headers[4] = '\xFF';  // Illegal  header id.
  QuicStreamFrame frame(kStreamId, false, 0, MakeIOVector(compressed_headers));

  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_INVALID_HEADER_ID));
  stream_->OnStreamFrame(frame);
}

TEST_F(QuicDataStreamTest, ProcessHeadersWithInvalidPriority) {
  Initialize(kShouldProcessData);

  string compressed_headers = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  compressed_headers[0] = '\xFF';  // Illegal priority.
  QuicStreamFrame frame(kStreamId, false, 0, MakeIOVector(compressed_headers));

  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_INVALID_PRIORITY));
  stream_->OnStreamFrame(frame);
}

TEST_F(QuicDataStreamTest, ProcessHeadersAndBody) {
  Initialize(kShouldProcessData);

  string compressed_headers = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  string body = "this is the body";
  string data = compressed_headers + body;
  QuicStreamFrame frame(kStreamId, false, 0, MakeIOVector(data));

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers_) + body,
            stream_->data());
}

TEST_F(QuicDataStreamTest, ProcessHeadersAndBodyFragments) {
  Initialize(kShouldProcessData);

  string compressed_headers = compressor_->CompressHeadersWithPriority(
      QuicUtils::LowestPriority(), headers_);
  string body = "this is the body";
  string data = compressed_headers + body;

  for (size_t fragment_size = 1; fragment_size < data.size(); ++fragment_size) {
    Initialize(kShouldProcessData);
    for (size_t offset = 0; offset < data.size(); offset += fragment_size) {
      size_t remaining_data = data.length() - offset;
      StringPiece fragment(data.data() + offset,
                           min(fragment_size, remaining_data));
      QuicStreamFrame frame(kStreamId, false, offset, MakeIOVector(fragment));

      stream_->OnStreamFrame(frame);
    }
    ASSERT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers_) + body,
              stream_->data()) << "fragment_size: " << fragment_size;
  }

  for (size_t split_point = 1; split_point < data.size() - 1; ++split_point) {
    Initialize(kShouldProcessData);

    StringPiece fragment1(data.data(), split_point);
    QuicStreamFrame frame1(kStreamId, false, 0, MakeIOVector(fragment1));
    stream_->OnStreamFrame(frame1);

    StringPiece fragment2(data.data() + split_point, data.size() - split_point);
    QuicStreamFrame frame2(
        kStreamId, false, split_point, MakeIOVector(fragment2));
    stream_->OnStreamFrame(frame2);

    ASSERT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers_) + body,
              stream_->data()) << "split_point: " << split_point;
  }
  EXPECT_EQ(QuicUtils::LowestPriority(), stream_->EffectivePriority());
}

TEST_F(QuicDataStreamTest, ProcessHeadersAndBodyReadv) {
  Initialize(!kShouldProcessData);

  string compressed_headers = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  string body = "this is the body";
  string data = compressed_headers + body;
  QuicStreamFrame frame(kStreamId, false, 0, MakeIOVector(data));
  string uncompressed_headers =
      SpdyUtils::SerializeUncompressedHeaders(headers_);
  string uncompressed_data = uncompressed_headers + body;

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(uncompressed_headers, stream_->data());

  char buffer[2048];
  ASSERT_LT(data.length(), arraysize(buffer));
  struct iovec vec;
  vec.iov_base = buffer;
  vec.iov_len = arraysize(buffer);

  size_t bytes_read = stream_->Readv(&vec, 1);
  EXPECT_EQ(uncompressed_headers.length(), bytes_read);
  EXPECT_EQ(uncompressed_headers, string(buffer, bytes_read));

  bytes_read = stream_->Readv(&vec, 1);
  EXPECT_EQ(body.length(), bytes_read);
  EXPECT_EQ(body, string(buffer, bytes_read));
}

TEST_F(QuicDataStreamTest, ProcessHeadersAndBodyIncrementalReadv) {
  Initialize(!kShouldProcessData);

  string compressed_headers = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  string body = "this is the body";
  string data = compressed_headers + body;
  QuicStreamFrame frame(kStreamId, false, 0, MakeIOVector(data));
  string uncompressed_headers =
      SpdyUtils::SerializeUncompressedHeaders(headers_);
  string uncompressed_data = uncompressed_headers + body;

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(uncompressed_headers, stream_->data());

  char buffer[1];
  struct iovec vec;
  vec.iov_base = buffer;
  vec.iov_len = arraysize(buffer);
  for (size_t i = 0; i < uncompressed_data.length(); ++i) {
    size_t bytes_read = stream_->Readv(&vec, 1);
    ASSERT_EQ(1u, bytes_read);
    EXPECT_EQ(uncompressed_data.data()[i], buffer[0]);
  }
}

TEST_F(QuicDataStreamTest, ProcessHeadersUsingReadvWithMultipleIovecs) {
  Initialize(!kShouldProcessData);

  string compressed_headers = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  string body = "this is the body";
  string data = compressed_headers + body;
  QuicStreamFrame frame(kStreamId, false, 0, MakeIOVector(data));
  string uncompressed_headers =
      SpdyUtils::SerializeUncompressedHeaders(headers_);
  string uncompressed_data = uncompressed_headers + body;

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(uncompressed_headers, stream_->data());

  char buffer1[1];
  char buffer2[1];
  struct iovec vec[2];
  vec[0].iov_base = buffer1;
  vec[0].iov_len = arraysize(buffer1);
  vec[1].iov_base = buffer2;
  vec[1].iov_len = arraysize(buffer2);
  for (size_t i = 0; i < uncompressed_data.length(); i += 2) {
    size_t bytes_read = stream_->Readv(vec, 2);
    ASSERT_EQ(2u, bytes_read) << i;
    ASSERT_EQ(uncompressed_data.data()[i], buffer1[0]) << i;
    ASSERT_EQ(uncompressed_data.data()[i + 1], buffer2[0]) << i;
  }
}

TEST_F(QuicDataStreamTest, ProcessCorruptHeadersEarly) {
  Initialize(kShouldProcessData);

  string compressed_headers1 = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  QuicStreamFrame frame1(
      stream_->id(), false, 0, MakeIOVector(compressed_headers1));
  string decompressed_headers1 =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  headers_["content-type"] = "text/plain";
  string compressed_headers2 = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  // Corrupt the compressed data.
  compressed_headers2[compressed_headers2.length() - 1] ^= 0xA1;
  QuicStreamFrame frame2(
      stream2_->id(), false, 0, MakeIOVector(compressed_headers2));
  string decompressed_headers2 =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  // Deliver frame2 to stream2 out of order.  The decompressor is not
  // available yet, so no data will be processed.  The compressed data
  // will be buffered until OnDecompressorAvailable() is called
  // to process it.
  stream2_->OnStreamFrame(frame2);
  EXPECT_EQ("", stream2_->data());

  // Now deliver frame1 to stream1.  The decompressor is available so
  // the data will be processed, and the decompressor will become
  // available for stream2.
  stream_->OnStreamFrame(frame1);
  EXPECT_EQ(decompressed_headers1, stream_->data());

  // Verify that the decompressor is available, and inform stream2
  // that it can now decompress the buffered compressed data.    Since
  // the compressed data is corrupt, the stream will shutdown the session.
  EXPECT_EQ(2u, session_->decompressor()->current_header_id());
  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_DECOMPRESSION_FAILURE));
  stream2_->OnDecompressorAvailable();
  EXPECT_EQ("", stream2_->data());
}

TEST_F(QuicDataStreamTest, ProcessPartialHeadersEarly) {
  Initialize(kShouldProcessData);

  string compressed_headers1 = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  QuicStreamFrame frame1(
      stream_->id(), false, 0, MakeIOVector(compressed_headers1));
  string decompressed_headers1 =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  headers_["content-type"] = "text/plain";
  string compressed_headers2 = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  string partial_compressed_headers =
      compressed_headers2.substr(0, compressed_headers2.length() / 2);
  QuicStreamFrame frame2(
      stream2_->id(), false, 0, MakeIOVector(partial_compressed_headers));
  string decompressed_headers2 =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  // Deliver frame2 to stream2 out of order.  The decompressor is not
  // available yet, so no data will be processed.  The compressed data
  // will be buffered until OnDecompressorAvailable() is called
  // to process it.
  stream2_->OnStreamFrame(frame2);
  EXPECT_EQ("", stream2_->data());

  // Now deliver frame1 to stream1.  The decompressor is available so
  // the data will be processed, and the decompressor will become
  // available for stream2.
  stream_->OnStreamFrame(frame1);
  EXPECT_EQ(decompressed_headers1, stream_->data());

  // Verify that the decompressor is available, and inform stream2
  // that it can now decompress the buffered compressed data.  Since
  // the compressed data is incomplete it will not be passed to
  // the stream.
  EXPECT_EQ(2u, session_->decompressor()->current_header_id());
  stream2_->OnDecompressorAvailable();
  EXPECT_EQ("", stream2_->data());

  // Now send remaining data and verify that we have now received the
  // compressed headers.
  string remaining_compressed_headers =
      compressed_headers2.substr(partial_compressed_headers.length());

  QuicStreamFrame frame3(stream2_->id(), false,
                         partial_compressed_headers.length(),
                         MakeIOVector(remaining_compressed_headers));
  stream2_->OnStreamFrame(frame3);
  EXPECT_EQ(decompressed_headers2, stream2_->data());
}

TEST_F(QuicDataStreamTest, ProcessHeadersEarly) {
  Initialize(kShouldProcessData);

  string compressed_headers1 = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  QuicStreamFrame frame1(
      stream_->id(), false, 0, MakeIOVector(compressed_headers1));
  string decompressed_headers1 =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  headers_["content-type"] = "text/plain";
  string compressed_headers2 = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  QuicStreamFrame frame2(
      stream2_->id(), false, 0, MakeIOVector(compressed_headers2));
  string decompressed_headers2 =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  // Deliver frame2 to stream2 out of order.  The decompressor is not
  // available yet, so no data will be processed.  The compressed data
  // will be buffered until OnDecompressorAvailable() is called
  // to process it.
  stream2_->OnStreamFrame(frame2);
  EXPECT_EQ("", stream2_->data());

  // Now deliver frame1 to stream1.  The decompressor is available so
  // the data will be processed, and the decompressor will become
  // available for stream2.
  stream_->OnStreamFrame(frame1);
  EXPECT_EQ(decompressed_headers1, stream_->data());

  // Verify that the decompressor is available, and inform stream2
  // that it can now decompress the buffered compressed data.
  EXPECT_EQ(2u, session_->decompressor()->current_header_id());
  stream2_->OnDecompressorAvailable();
  EXPECT_EQ(decompressed_headers2, stream2_->data());
}

TEST_F(QuicDataStreamTest, ProcessHeadersDelay) {
  Initialize(!kShouldProcessData);

  string compressed_headers = compressor_->CompressHeadersWithPriority(
      QuicUtils::HighestPriority(), headers_);
  QuicStreamFrame frame1(
      stream_->id(), false, 0, MakeIOVector(compressed_headers));
  string decompressed_headers =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  // Send the headers to the stream and verify they were decompressed.
  stream_->OnStreamFrame(frame1);
  EXPECT_EQ(2u, session_->decompressor()->current_header_id());

  // Verify that we are now able to handle the body data,
  // even though the stream has not processed the headers.
  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_INVALID_HEADER_ID))
      .Times(0);
  QuicStreamFrame frame2(stream_->id(), false, compressed_headers.length(),
                         MakeIOVector("body data"));
  stream_->OnStreamFrame(frame2);
}

}  // namespace
}  // namespace test
}  // namespace net
