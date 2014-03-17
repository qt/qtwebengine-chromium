// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/quota/quota_reservation_buffer.h"

#include "base/bind.h"
#include "webkit/browser/fileapi/quota/open_file_handle.h"
#include "webkit/browser/fileapi/quota/open_file_handle_context.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"

namespace fileapi {

QuotaReservationBuffer::QuotaReservationBuffer(
    base::WeakPtr<QuotaReservationManager> reservation_manager,
    const GURL& origin,
    FileSystemType type)
    : reservation_manager_(reservation_manager),
      origin_(origin),
      type_(type),
      reserved_quota_(0) {
  DCHECK(origin.is_valid());
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  reservation_manager_->IncrementDirtyCount(origin, type);
}

scoped_refptr<QuotaReservation> QuotaReservationBuffer::CreateReservation() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return make_scoped_refptr(new QuotaReservation(this));
}

scoped_ptr<OpenFileHandle> QuotaReservationBuffer::GetOpenFileHandle(
    QuotaReservation* reservation,
    const base::FilePath& platform_path) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  OpenFileHandleContext** open_file = &open_files_[platform_path];
  if (!*open_file)
    *open_file = new OpenFileHandleContext(platform_path, this);
  return make_scoped_ptr(new OpenFileHandle(reservation, *open_file));
}

void QuotaReservationBuffer::CommitFileGrowth(int64 quota_consumption,
                                              int64 usage_delta) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!reservation_manager_)
    return;
  reservation_manager_->CommitQuotaUsage(origin_, type_, usage_delta);

  if (quota_consumption > 0) {
    if (quota_consumption > reserved_quota_) {
      LOG(ERROR) << "Detected over consumption of the storage quota beyond its"
                 << " reservation";
      quota_consumption = reserved_quota_;
    }

    reserved_quota_ -= quota_consumption;
    reservation_manager_->ReleaseReservedQuota(
        origin_, type_, quota_consumption);
  }
}

void QuotaReservationBuffer::DetachOpenFileHandleContext(
    OpenFileHandleContext* open_file) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_EQ(open_file, open_files_[open_file->platform_path()]);
  open_files_.erase(open_file->platform_path());
}

void QuotaReservationBuffer::PutReservationToBuffer(int64 reservation) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK_LE(0, reservation);
  reserved_quota_ += reservation;
}

QuotaReservationBuffer::~QuotaReservationBuffer() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  if (!reservation_manager_)
    return;

  DCHECK_LE(0, reserved_quota_);
  if (reserved_quota_ && reservation_manager_) {
    reservation_manager_->ReserveQuota(
        origin_, type_, -reserved_quota_,
        base::Bind(&QuotaReservationBuffer::DecrementDirtyCount,
                   reservation_manager_, origin_, type_));
  }
  reservation_manager_->ReleaseReservationBuffer(this);
}

// static
bool QuotaReservationBuffer::DecrementDirtyCount(
    base::WeakPtr<QuotaReservationManager> reservation_manager,
    const GURL& origin,
    FileSystemType type,
    base::PlatformFileError error) {
  DCHECK(origin.is_valid());
  if (error == base::PLATFORM_FILE_OK && reservation_manager) {
    reservation_manager->DecrementDirtyCount(origin, type);
    return true;
  }
  return false;
}


}  // namespace fileapi
