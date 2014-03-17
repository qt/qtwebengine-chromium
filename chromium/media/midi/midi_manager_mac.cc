// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_mac.h"

#include <string>

#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"

#include <CoreAudio/HostTime.h>

using base::IntToString;
using base::SysCFStringRefToUTF8;
using std::string;

// NB: System MIDI types are pointer types in 32-bit and integer types in
// 64-bit. Therefore, the initialization is the simplest one that satisfies both
// (if possible).

namespace media {

MIDIManager* MIDIManager::Create() {
  return new MIDIManagerMac();
}

MIDIManagerMac::MIDIManagerMac()
    : midi_client_(0),
      coremidi_input_(0),
      coremidi_output_(0),
      packet_list_(NULL),
      midi_packet_(NULL),
      send_thread_("MIDISendThread") {
}

bool MIDIManagerMac::Initialize() {
  TRACE_EVENT0("midi", "MIDIManagerMac::Initialize");

  // CoreMIDI registration.
  midi_client_ = 0;
  OSStatus result = MIDIClientCreate(
      CFSTR("Google Chrome"),
      NULL,
      NULL,
      &midi_client_);

  if (result != noErr)
    return false;

  coremidi_input_ = 0;

  // Create input and output port.
  result = MIDIInputPortCreate(
      midi_client_,
      CFSTR("MIDI Input"),
      ReadMIDIDispatch,
      this,
      &coremidi_input_);
  if (result != noErr)
    return false;

  result = MIDIOutputPortCreate(
      midi_client_,
      CFSTR("MIDI Output"),
      &coremidi_output_);
  if (result != noErr)
    return false;

  uint32 destination_count = MIDIGetNumberOfDestinations();
  destinations_.resize(destination_count);

  for (uint32 i = 0; i < destination_count ; i++) {
    MIDIEndpointRef destination = MIDIGetDestination(i);

    // Keep track of all destinations (known as outputs by the Web MIDI API).
    // Cache to avoid any possible overhead in calling MIDIGetDestination().
    destinations_[i] = destination;

    MIDIPortInfo info = GetPortInfoFromEndpoint(destination);
    AddOutputPort(info);
  }

  // Open connections from all sources.
  uint32 source_count = MIDIGetNumberOfSources();

  for (uint32 i = 0; i < source_count; ++i)  {
    // Receive from all sources.
    MIDIEndpointRef src = MIDIGetSource(i);
    MIDIPortConnectSource(coremidi_input_, src, reinterpret_cast<void*>(src));

    // Keep track of all sources (known as inputs in Web MIDI API terminology).
    source_map_[src] = i;

    MIDIPortInfo info = GetPortInfoFromEndpoint(src);
    AddInputPort(info);
  }

  // TODO(crogers): Fix the memory management here!
  packet_list_ = reinterpret_cast<MIDIPacketList*>(midi_buffer_);
  midi_packet_ = MIDIPacketListInit(packet_list_);

  return true;
}

void MIDIManagerMac::DispatchSendMIDIData(MIDIManagerClient* client,
                                          uint32 port_index,
                                          const std::vector<uint8>& data,
                                          double timestamp) {
  if (!send_thread_.IsRunning())
    send_thread_.Start();

  // OK to use base::Unretained(this) since we join to thread in dtor().
  send_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MIDIManagerMac::SendMIDIData, base::Unretained(this),
                 client, port_index, data, timestamp));
}

MIDIManagerMac::~MIDIManagerMac() {
  // Wait for the termination of |send_thread_| before disposing MIDI ports.
  send_thread_.Stop();

  if (coremidi_input_)
    MIDIPortDispose(coremidi_input_);
  if (coremidi_output_)
    MIDIPortDispose(coremidi_output_);
}

// static
void MIDIManagerMac::ReadMIDIDispatch(const MIDIPacketList* packet_list,
                                      void* read_proc_refcon,
                                      void* src_conn_refcon) {
  MIDIManagerMac* manager = static_cast<MIDIManagerMac*>(read_proc_refcon);
#if __LP64__
  MIDIEndpointRef source = reinterpret_cast<uintptr_t>(src_conn_refcon);
#else
  MIDIEndpointRef source = static_cast<MIDIEndpointRef>(src_conn_refcon);
#endif

  // Dispatch to class method.
  manager->ReadMIDI(source, packet_list);
}

void MIDIManagerMac::ReadMIDI(MIDIEndpointRef source,
                              const MIDIPacketList* packet_list) {
  // Lookup the port index based on the source.
  SourceMap::iterator j = source_map_.find(source);
  if (j == source_map_.end())
    return;
  uint32 port_index = source_map_[source];

  // Go through each packet and process separately.
  for (size_t i = 0; i < packet_list->numPackets; i++) {
    // Each packet contains MIDI data for one or more messages (like note-on).
    const MIDIPacket &packet = packet_list->packet[i];
    double timestamp_seconds = MIDITimeStampToSeconds(packet.timeStamp);

    ReceiveMIDIData(
        port_index,
        packet.data,
        packet.length,
        timestamp_seconds);
  }
}

void MIDIManagerMac::SendMIDIData(MIDIManagerClient* client,
                                  uint32 port_index,
                                  const std::vector<uint8>& data,
                                  double timestamp) {
  DCHECK(send_thread_.message_loop_proxy()->BelongsToCurrentThread());

  // System Exclusive has already been filtered.
  MIDITimeStamp coremidi_timestamp = SecondsToMIDITimeStamp(timestamp);

  midi_packet_ = MIDIPacketListAdd(
      packet_list_,
      kMaxPacketListSize,
      midi_packet_,
      coremidi_timestamp,
      data.size(),
      &data[0]);

  // Lookup the destination based on the port index.
  if (static_cast<size_t>(port_index) >= destinations_.size())
    return;

  MIDIEndpointRef destination = destinations_[port_index];

  MIDISend(coremidi_output_, destination, packet_list_);

  // Re-initialize for next time.
  midi_packet_ = MIDIPacketListInit(packet_list_);

  client->AccumulateMIDIBytesSent(data.size());
}

// static
MIDIPortInfo MIDIManagerMac::GetPortInfoFromEndpoint(
    MIDIEndpointRef endpoint) {
  SInt32 id_number = 0;
  MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &id_number);
  string id = IntToString(id_number);

  string manufacturer;
  CFStringRef manufacturer_ref = NULL;
  OSStatus result = MIDIObjectGetStringProperty(
      endpoint, kMIDIPropertyManufacturer, &manufacturer_ref);
  if (result == noErr) {
    manufacturer = SysCFStringRefToUTF8(manufacturer_ref);
  } else {
    // kMIDIPropertyManufacturer is not supported in IAC driver providing
    // endpoints, and the result will be kMIDIUnknownProperty (-10835).
    DLOG(WARNING) << "Failed to get kMIDIPropertyManufacturer with status "
                  << result;
  }

  string name;
  CFStringRef name_ref = NULL;
  result = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name_ref);
  if (result == noErr)
    name = SysCFStringRefToUTF8(name_ref);
  else
    DLOG(WARNING) << "Failed to get kMIDIPropertyName with status " << result;

  string version;
  SInt32 version_number = 0;
  result = MIDIObjectGetIntegerProperty(
      endpoint, kMIDIPropertyDriverVersion, &version_number);
  if (result == noErr) {
    version = IntToString(version_number);
  } else {
    // kMIDIPropertyDriverVersion is not supported in IAC driver providing
    // endpoints, and the result will be kMIDIUnknownProperty (-10835).
    DLOG(WARNING) << "Failed to get kMIDIPropertyDriverVersion with status "
                  << result;
  }

  return MIDIPortInfo(id, manufacturer, name, version);
}

// static
double MIDIManagerMac::MIDITimeStampToSeconds(MIDITimeStamp timestamp) {
  UInt64 nanoseconds = AudioConvertHostTimeToNanos(timestamp);
  return static_cast<double>(nanoseconds) / 1.0e9;
}

// static
MIDITimeStamp MIDIManagerMac::SecondsToMIDITimeStamp(double seconds) {
  UInt64 nanos = UInt64(seconds * 1.0e9);
  return AudioConvertNanosToHostTime(nanos);
}

}  // namespace media
