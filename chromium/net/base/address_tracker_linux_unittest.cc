// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_tracker_linux.h"

#include <linux/if.h>

#include <vector>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace internal {

typedef std::vector<char> Buffer;

void Noop() {}

class AddressTrackerLinuxTest : public testing::Test {
 protected:
  AddressTrackerLinuxTest() : tracker_(base::Bind(&Noop), base::Bind(&Noop)) {}

  bool HandleAddressMessage(const Buffer& buf) {
    bool address_changed = false;
    bool link_changed = false;
    tracker_.HandleMessage(&buf[0], buf.size(),
                           &address_changed, &link_changed);
    EXPECT_FALSE(link_changed);
    return address_changed;
  }

  bool HandleLinkMessage(const Buffer& buf) {
    bool address_changed = false;
    bool link_changed = false;
    tracker_.HandleMessage(&buf[0], buf.size(),
                           &address_changed, &link_changed);
    EXPECT_FALSE(address_changed);
    return link_changed;
  }

  AddressTrackerLinux::AddressMap GetAddressMap() {
    return tracker_.GetAddressMap();
  }

  const base::hash_set<int>* GetOnlineLinks() const {
    return &tracker_.online_links_;
  }

  AddressTrackerLinux tracker_;
};

namespace {

class NetlinkMessage {
 public:
  explicit NetlinkMessage(uint16 type) : buffer_(NLMSG_HDRLEN) {
    header()->nlmsg_type = type;
    Align();
  }

  void AddPayload(const void* data, size_t length) {
    CHECK_EQ(static_cast<size_t>(NLMSG_HDRLEN),
             buffer_.size()) << "Payload must be added first";
    Append(data, length);
    Align();
  }

  void AddAttribute(uint16 type, const void* data, size_t length) {
    struct nlattr attr;
    attr.nla_len = NLA_HDRLEN + length;
    attr.nla_type = type;
    Append(&attr, sizeof(attr));
    Align();
    Append(data, length);
    Align();
  }

  void AppendTo(Buffer* output) const {
    CHECK_EQ(NLMSG_ALIGN(output->size()), output->size());
    output->reserve(output->size() + NLMSG_LENGTH(buffer_.size()));
    output->insert(output->end(), buffer_.begin(), buffer_.end());
  }

 private:
  void Append(const void* data, size_t length) {
    const char* chardata = reinterpret_cast<const char*>(data);
    buffer_.insert(buffer_.end(), chardata, chardata + length);
  }

  void Align() {
    header()->nlmsg_len = buffer_.size();
    buffer_.insert(buffer_.end(), NLMSG_ALIGN(buffer_.size()) - buffer_.size(),
                   0);
    CHECK(NLMSG_OK(header(), buffer_.size()));
  }

  struct nlmsghdr* header() {
    return reinterpret_cast<struct nlmsghdr*>(&buffer_[0]);
  }

  Buffer buffer_;
};

void MakeAddrMessage(uint16 type,
                     uint8 flags,
                     uint8 family,
                     const IPAddressNumber& address,
                     const IPAddressNumber& local,
                     Buffer* output) {
  NetlinkMessage nlmsg(type);
  struct ifaddrmsg msg = {};
  msg.ifa_family = family;
  msg.ifa_flags = flags;
  nlmsg.AddPayload(&msg, sizeof(msg));
  if (address.size())
    nlmsg.AddAttribute(IFA_ADDRESS, &address[0], address.size());
  if (local.size())
    nlmsg.AddAttribute(IFA_LOCAL, &local[0], local.size());
  nlmsg.AppendTo(output);
}

void MakeLinkMessage(uint16 type, uint32 flags, uint32 index, Buffer* output) {
  NetlinkMessage nlmsg(type);
  struct ifinfomsg msg = {};
  msg.ifi_index = index;
  msg.ifi_flags = flags;
  nlmsg.AddPayload(&msg, sizeof(msg));
  output->clear();
  nlmsg.AppendTo(output);
}

const unsigned char kAddress0[] = { 127, 0, 0, 1 };
const unsigned char kAddress1[] = { 10, 0, 0, 1 };
const unsigned char kAddress2[] = { 192, 168, 0, 1 };
const unsigned char kAddress3[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 1 };

TEST_F(AddressTrackerLinuxTest, NewAddress) {
  const IPAddressNumber kEmpty;
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));
  const IPAddressNumber kAddr1(kAddress1, kAddress1 + arraysize(kAddress1));
  const IPAddressNumber kAddr2(kAddress2, kAddress2 + arraysize(kAddress2));
  const IPAddressNumber kAddr3(kAddress3, kAddress3 + arraysize(kAddress3));

  Buffer buffer;
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kAddr0, kEmpty,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  buffer.clear();
  MakeAddrMessage(RTM_NEWADDR, IFA_F_HOMEADDRESS, AF_INET, kAddr1, kAddr2,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(2u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(1u, map.count(kAddr2));
  EXPECT_EQ(IFA_F_HOMEADDRESS, map[kAddr2].ifa_flags);

  buffer.clear();
  MakeAddrMessage(RTM_NEWADDR, 0, AF_INET6, kEmpty, kAddr3, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(3u, map.size());
  EXPECT_EQ(1u, map.count(kAddr3));
}

TEST_F(AddressTrackerLinuxTest, NewAddressChange) {
  const IPAddressNumber kEmpty;
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));

  Buffer buffer;
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kAddr0, kEmpty,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  buffer.clear();
  MakeAddrMessage(RTM_NEWADDR, IFA_F_HOMEADDRESS, AF_INET, kAddr0, kEmpty,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(IFA_F_HOMEADDRESS, map[kAddr0].ifa_flags);

  // Both messages in one buffer.
  buffer.clear();
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kAddr0, kEmpty,
                  &buffer);
  MakeAddrMessage(RTM_NEWADDR, IFA_F_HOMEADDRESS, AF_INET, kAddr0, kEmpty,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(IFA_F_HOMEADDRESS, map[kAddr0].ifa_flags);
}

TEST_F(AddressTrackerLinuxTest, NewAddressDuplicate) {
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));

  Buffer buffer;
  MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kAddr0, kAddr0,
                  &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1u, map.count(kAddr0));
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  EXPECT_FALSE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);
}

TEST_F(AddressTrackerLinuxTest, DeleteAddress) {
  const IPAddressNumber kEmpty;
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));
  const IPAddressNumber kAddr1(kAddress1, kAddress1 + arraysize(kAddress1));
  const IPAddressNumber kAddr2(kAddress2, kAddress2 + arraysize(kAddress2));

  Buffer buffer;
  MakeAddrMessage(RTM_NEWADDR, 0, AF_INET, kAddr0, kEmpty, &buffer);
  MakeAddrMessage(RTM_NEWADDR, 0, AF_INET, kAddr1, kAddr2, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(2u, map.size());

  buffer.clear();
  MakeAddrMessage(RTM_DELADDR, 0, AF_INET, kEmpty, kAddr0, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(0u, map.count(kAddr0));
  EXPECT_EQ(1u, map.count(kAddr2));

  buffer.clear();
  MakeAddrMessage(RTM_DELADDR, 0, AF_INET, kAddr2, kAddr1, &buffer);
  // kAddr1 does not exist in the map.
  EXPECT_FALSE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());

  buffer.clear();
  MakeAddrMessage(RTM_DELADDR, 0, AF_INET, kAddr2, kEmpty, &buffer);
  EXPECT_TRUE(HandleAddressMessage(buffer));
  map = GetAddressMap();
  EXPECT_EQ(0u, map.size());
}

TEST_F(AddressTrackerLinuxTest, IgnoredMessage) {
  const IPAddressNumber kEmpty;
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));
  const IPAddressNumber kAddr3(kAddress3, kAddress3 + arraysize(kAddress3));

  Buffer buffer;
  // Ignored family.
  MakeAddrMessage(RTM_NEWADDR, 0, AF_UNSPEC, kAddr3, kAddr0, &buffer);
  // No address.
  MakeAddrMessage(RTM_NEWADDR, 0, AF_INET, kEmpty, kEmpty, &buffer);
  // Ignored type.
  MakeAddrMessage(RTM_DELROUTE, 0, AF_INET6, kAddr3, kEmpty, &buffer);
  EXPECT_FALSE(HandleAddressMessage(buffer));
  EXPECT_TRUE(GetAddressMap().empty());

  // Valid message after ignored messages.
  NetlinkMessage nlmsg(RTM_NEWADDR);
  struct ifaddrmsg msg = {};
  msg.ifa_family = AF_INET;
  nlmsg.AddPayload(&msg, sizeof(msg));
  // Ignored attribute.
  struct ifa_cacheinfo cache_info = {};
  nlmsg.AddAttribute(IFA_CACHEINFO, &cache_info, sizeof(cache_info));
  nlmsg.AddAttribute(IFA_ADDRESS, &kAddr0[0], kAddr0.size());
  nlmsg.AppendTo(&buffer);

  EXPECT_TRUE(HandleAddressMessage(buffer));
  EXPECT_EQ(1u, GetAddressMap().size());
}

TEST_F(AddressTrackerLinuxTest, AddInterface) {
  Buffer buffer;

  // Ignores loopback.
  MakeLinkMessage(RTM_NEWLINK,
                  IFF_LOOPBACK | IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                  0, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks()->empty());

  // Ignores not IFF_LOWER_UP.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks()->empty());

  // Ignores deletion.
  MakeLinkMessage(RTM_DELLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks()->empty());

  // Verify success.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_EQ(1u, GetOnlineLinks()->count(0));
  EXPECT_EQ(1u, GetOnlineLinks()->size());

  // Ignores redundant enables.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_EQ(1u, GetOnlineLinks()->count(0));
  EXPECT_EQ(1u, GetOnlineLinks()->size());

  // Verify adding another online device (e.g. VPN) is considered a change.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING, 1, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_EQ(1u, GetOnlineLinks()->count(0));
  EXPECT_EQ(1u, GetOnlineLinks()->count(1));
  EXPECT_EQ(2u, GetOnlineLinks()->size());
}

TEST_F(AddressTrackerLinuxTest, RemoveInterface) {
  Buffer buffer;

  // Should disappear when not IFF_LOWER_UP.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_FALSE(GetOnlineLinks()->empty());
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks()->empty());

  // Ignores redundant disables.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks()->empty());

  // Ignores deleting down interfaces.
  MakeLinkMessage(RTM_DELLINK, IFF_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_FALSE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks()->empty());

  // Should disappear when deleted.
  MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_FALSE(GetOnlineLinks()->empty());
  MakeLinkMessage(RTM_DELLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING, 0, &buffer);
  EXPECT_TRUE(HandleLinkMessage(buffer));
  EXPECT_TRUE(GetOnlineLinks()->empty());
}

}  // namespace

}  // namespace internal
}  // namespace net
