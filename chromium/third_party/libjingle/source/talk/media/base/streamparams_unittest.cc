/*
 * libjingle
 * Copyright 2012 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/gunit.h"
#include "talk/media/base/streamparams.h"
#include "talk/media/base/testutils.h"

static const uint32 kSsrcs1[] = {1};
static const uint32 kSsrcs2[] = {1, 2};
static const uint32 kSsrcs3[] = {1, 2, 3};
static const uint32 kRtxSsrcs3[] = {4, 5, 6};

static cricket::StreamParams CreateStreamParamsWithSsrcGroup(
    const std::string& semantics, const uint32 ssrcs_in[], size_t len) {
  cricket::StreamParams stream;
  std::vector<uint32> ssrcs(ssrcs_in, ssrcs_in + len);
  cricket::SsrcGroup sg(semantics, ssrcs);
  stream.ssrcs = ssrcs;
  stream.ssrc_groups.push_back(sg);
  return stream;
}

TEST(SsrcGroup, EqualNotEqual) {
  cricket::SsrcGroup ssrc_groups[] = {
    cricket::SsrcGroup("ABC", MAKE_VECTOR(kSsrcs1)),
    cricket::SsrcGroup("ABC", MAKE_VECTOR(kSsrcs2)),
    cricket::SsrcGroup("Abc", MAKE_VECTOR(kSsrcs2)),
    cricket::SsrcGroup("abc", MAKE_VECTOR(kSsrcs2)),
  };

  for (size_t i = 0; i < ARRAY_SIZE(ssrc_groups); ++i) {
    for (size_t j = 0; j < ARRAY_SIZE(ssrc_groups); ++j) {
      EXPECT_EQ((ssrc_groups[i] == ssrc_groups[j]), (i == j));
      EXPECT_EQ((ssrc_groups[i] != ssrc_groups[j]), (i != j));
    }
  }
}

TEST(SsrcGroup, HasSemantics) {
  cricket::SsrcGroup sg1("ABC", MAKE_VECTOR(kSsrcs1));
  EXPECT_TRUE(sg1.has_semantics("ABC"));

  cricket::SsrcGroup sg2("Abc", MAKE_VECTOR(kSsrcs1));
  EXPECT_FALSE(sg2.has_semantics("ABC"));

  cricket::SsrcGroup sg3("abc", MAKE_VECTOR(kSsrcs1));
  EXPECT_FALSE(sg3.has_semantics("ABC"));
}

TEST(SsrcGroup, ToString) {
  cricket::SsrcGroup sg1("ABC", MAKE_VECTOR(kSsrcs1));
  EXPECT_STREQ("{semantics:ABC;ssrcs:[1]}", sg1.ToString().c_str());
}

TEST(StreamParams, CreateLegacy) {
  const uint32 ssrc = 7;
  cricket::StreamParams one_sp = cricket::StreamParams::CreateLegacy(ssrc);
  EXPECT_EQ(1U, one_sp.ssrcs.size());
  EXPECT_EQ(ssrc, one_sp.first_ssrc());
  EXPECT_TRUE(one_sp.has_ssrcs());
  EXPECT_TRUE(one_sp.has_ssrc(ssrc));
  EXPECT_FALSE(one_sp.has_ssrc(ssrc+1));
  EXPECT_FALSE(one_sp.has_ssrc_groups());
  EXPECT_EQ(0U, one_sp.ssrc_groups.size());
}

TEST(StreamParams, HasSsrcGroup) {
  cricket::StreamParams sp =
      CreateStreamParamsWithSsrcGroup("XYZ", kSsrcs2, ARRAY_SIZE(kSsrcs2));
  EXPECT_EQ(2U, sp.ssrcs.size());
  EXPECT_EQ(kSsrcs2[0], sp.first_ssrc());
  EXPECT_TRUE(sp.has_ssrcs());
  EXPECT_TRUE(sp.has_ssrc(kSsrcs2[0]));
  EXPECT_TRUE(sp.has_ssrc(kSsrcs2[1]));
  EXPECT_TRUE(sp.has_ssrc_group("XYZ"));
  EXPECT_EQ(1U, sp.ssrc_groups.size());
  EXPECT_EQ(2U, sp.ssrc_groups[0].ssrcs.size());
  EXPECT_EQ(kSsrcs2[0], sp.ssrc_groups[0].ssrcs[0]);
  EXPECT_EQ(kSsrcs2[1], sp.ssrc_groups[0].ssrcs[1]);
}

TEST(StreamParams, GetSsrcGroup) {
  cricket::StreamParams sp =
      CreateStreamParamsWithSsrcGroup("XYZ", kSsrcs2, ARRAY_SIZE(kSsrcs2));
  EXPECT_EQ(NULL, sp.get_ssrc_group("xyz"));
  EXPECT_EQ(&sp.ssrc_groups[0], sp.get_ssrc_group("XYZ"));
}

TEST(StreamParams, EqualNotEqual) {
  cricket::StreamParams l1 = cricket::StreamParams::CreateLegacy(1);
  cricket::StreamParams l2 = cricket::StreamParams::CreateLegacy(2);
  cricket::StreamParams sg1 =
      CreateStreamParamsWithSsrcGroup("ABC", kSsrcs1, ARRAY_SIZE(kSsrcs1));
  cricket::StreamParams sg2 =
      CreateStreamParamsWithSsrcGroup("ABC", kSsrcs2, ARRAY_SIZE(kSsrcs2));
  cricket::StreamParams sg3 =
      CreateStreamParamsWithSsrcGroup("Abc", kSsrcs2, ARRAY_SIZE(kSsrcs2));
  cricket::StreamParams sg4 =
      CreateStreamParamsWithSsrcGroup("abc", kSsrcs2, ARRAY_SIZE(kSsrcs2));
  cricket::StreamParams sps[] = {l1, l2, sg1, sg2, sg3, sg4};

  for (size_t i = 0; i < ARRAY_SIZE(sps); ++i) {
    for (size_t j = 0; j < ARRAY_SIZE(sps); ++j) {
      EXPECT_EQ((sps[i] == sps[j]), (i == j));
      EXPECT_EQ((sps[i] != sps[j]), (i != j));
    }
  }
}

TEST(StreamParams, FidFunctions) {
  uint32 fid_ssrc;

  cricket::StreamParams sp = cricket::StreamParams::CreateLegacy(1);
  EXPECT_FALSE(sp.AddFidSsrc(10, 20));
  EXPECT_TRUE(sp.AddFidSsrc(1, 2));
  EXPECT_TRUE(sp.GetFidSsrc(1, &fid_ssrc));
  EXPECT_EQ(2u, fid_ssrc);
  EXPECT_FALSE(sp.GetFidSsrc(15, &fid_ssrc));

  sp.add_ssrc(20);
  sp.AddFidSsrc(20, 30);
  EXPECT_TRUE(sp.GetFidSsrc(20, &fid_ssrc));
  EXPECT_EQ(30u, fid_ssrc);

  // Manually create SsrcGroup to test bounds-checking
  // in GetSecondarySsrc. We construct an invalid StreamParams
  // for this.
  std::vector<uint32> fid_vector;
  fid_vector.push_back(13);
  cricket::SsrcGroup invalid_fid_group(cricket::kFidSsrcGroupSemantics,
                                        fid_vector);
  cricket::StreamParams sp_invalid;
  sp_invalid.add_ssrc(13);
  sp_invalid.ssrc_groups.push_back(invalid_fid_group);
  EXPECT_FALSE(sp_invalid.GetFidSsrc(13, &fid_ssrc));
}

TEST(StreamParams, ToString) {
  cricket::StreamParams sp =
      CreateStreamParamsWithSsrcGroup("XYZ", kSsrcs2, ARRAY_SIZE(kSsrcs2));
  EXPECT_STREQ("{ssrcs:[1,2];ssrc_groups:{semantics:XYZ;ssrcs:[1,2]};}",
               sp.ToString().c_str());
}


TEST(StreamParams, TestIsOneSsrcStream_LegacyStream) {
  EXPECT_TRUE(
      cricket::IsOneSsrcStream(cricket::StreamParams::CreateLegacy(13)));
}

TEST(StreamParams, TestIsOneSsrcStream_SingleRtxStream) {
  cricket::StreamParams stream;
  stream.add_ssrc(13);
  EXPECT_TRUE(stream.AddFidSsrc(13, 14));
  EXPECT_TRUE(cricket::IsOneSsrcStream(stream));
}

TEST(StreamParams, TestIsOneSsrcStream_SimulcastStream) {
  EXPECT_FALSE(cricket::IsOneSsrcStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
  EXPECT_FALSE(cricket::IsOneSsrcStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));
}

TEST(StreamParams, TestIsOneSsrcStream_SimRtxStream) {
  cricket::StreamParams stream =
      cricket::CreateSimWithRtxStreamParams("cname",
                                            MAKE_VECTOR(kSsrcs3),
                                            MAKE_VECTOR(kRtxSsrcs3));
  EXPECT_FALSE(cricket::IsOneSsrcStream(stream));
}

TEST(StreamParams, TestIsSimulcastStream_LegacyStream) {
  EXPECT_FALSE(
      cricket::IsSimulcastStream(cricket::StreamParams::CreateLegacy(13)));
}

TEST(StreamParams, TestIsSimulcastStream_SingleRtxStream) {
  cricket::StreamParams stream;
  stream.add_ssrc(13);
  EXPECT_TRUE(stream.AddFidSsrc(13, 14));
  EXPECT_FALSE(cricket::IsSimulcastStream(stream));
}

TEST(StreamParams, TestIsSimulcastStream_SimulcastStream) {
  EXPECT_TRUE(cricket::IsSimulcastStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2))));
  EXPECT_TRUE(cricket::IsSimulcastStream(
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs3))));
}

TEST(StreamParams, TestIsSimulcastStream_SimRtxStream) {
  cricket::StreamParams stream =
      cricket::CreateSimWithRtxStreamParams("cname",
                                            MAKE_VECTOR(kSsrcs3),
                                            MAKE_VECTOR(kRtxSsrcs3));
  EXPECT_TRUE(cricket::IsSimulcastStream(stream));
}

TEST(StreamParams, TestIsSimulcastStream_InvalidStreams) {
  // stream1 has extra non-sim, non-fid ssrc.
  cricket::StreamParams stream1 =
      cricket::CreateSimWithRtxStreamParams("cname",
                                            MAKE_VECTOR(kSsrcs3),
                                            MAKE_VECTOR(kRtxSsrcs3));
  stream1.add_ssrc(25);
  EXPECT_FALSE(cricket::IsSimulcastStream(stream1));

  // stream2 has invalid fid-group (no primary).
  cricket::StreamParams stream2;
  stream2.add_ssrc(13);
  EXPECT_TRUE(stream2.AddFidSsrc(13, 14));
  std::remove(stream2.ssrcs.begin(), stream2.ssrcs.end(), 13);
  EXPECT_FALSE(cricket::IsSimulcastStream(stream2));

  // stream3 has two SIM groups.
  cricket::StreamParams stream3 =
      cricket::CreateSimStreamParams("cname", MAKE_VECTOR(kSsrcs2));
  std::vector<uint32> sim_ssrcs = MAKE_VECTOR(kRtxSsrcs3);
  cricket::SsrcGroup sg(cricket::kSimSsrcGroupSemantics, sim_ssrcs);
  for (size_t i = 0; i < sim_ssrcs.size(); i++) {
    stream3.add_ssrc(sim_ssrcs[i]);
  }
  stream3.ssrc_groups.push_back(sg);
  EXPECT_FALSE(cricket::IsSimulcastStream(stream3));
}
