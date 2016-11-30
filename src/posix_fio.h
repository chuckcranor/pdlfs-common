#pragma once

/*
 * Copyright (c) 2015-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "posix_env.h"
#include "posix_fio.h"

namespace pdlfs {

class PosixFio : public Fio {
  static std::string ToFileName(const Slice& encoding) {
    Slice key_prefix = Fentry::ExtractUntypedKeyPrefix(encoding);
    char tmp[200];
    sprintf(tmp, "F_");
    char* p = tmp + 2;
    for (size_t i = 0; i < key_prefix.size(); i++) {
      sprintf(p, "%02X", (unsigned char)key_prefix[i]);
      p += 2;
    }
    return tmp;
  }

 public:
  explicit PosixFio(const Slice& root) {
    Env::Default()->CreateDir(root);
    root_ = root.ToString();
  }

  virtual ~PosixFio() {}

  virtual Status Creat(const Slice& fentry, Handle** fh) {
    Status s;
    std::string fname = root_ + "/";
    fname += ToFileName(fentry);
    int fd = open(fname.c_str(), O_RDWR | O_CREAT | O_TRUNC, DEFFILEMODE);
    if (fd != -1) {
      *fh = reinterpret_cast<Handle*>(fd);
    } else {
      s = IOError(fname, errno);
    }

#if VERBOSE >= 10
    if (s.ok()) {
      Slice encoding = fentry;
      Fentry ent;
      if (ent.DecodeFrom(&encoding)) {
        Verbose(__LOG_ARGS__, 10, "posix_creat: [%llu:%llu:%llu] -> %s",
                (unsigned long long)ent.stat.RegId(),
                (unsigned long long)ent.stat.SnapId(),
                (unsigned long long)ent.stat.InodeNo(), fname.c_str());
      }
    }
#endif

    return s;
  }

  virtual Status Open(const Slice& fentry, bool create_if_missing,
                      bool truncate_if_exists, uint64_t* mtime, uint64_t* size,
                      Handle** fh) {
    Status s;
    std::string fname = root_ + "/";
    fname += ToFileName(fentry);
    int flags = O_RDWR;
    if (truncate_if_exists) {
      flags |= O_TRUNC;
    }
    if (create_if_missing) {
      flags |= O_CREAT;
    }
    int fd = open(fname.c_str(), flags, DEFFILEMODE);
    if (fd != -1) {
      struct stat buf;
      int r = fstat(fd, &buf);
      if (r == 0) {
        *fh = reinterpret_cast<Handle*>(fd);
        *mtime = 1000ULL * 1000ULL * buf.st_mtime;
        *size = buf.st_size;
      } else {
        s = IOError(fname, errno);
      }
    } else {
      s = IOError(fname, errno);
    }

#if VERBOSE >= 10
    if (s.ok()) {
      Slice encoding = fentry;
      Fentry ent;
      if (ent.DecodeFrom(&encoding)) {
        Verbose(__LOG_ARGS__, 10, "posix_open: [%llu:%llu:%llu] -> %s",
                (unsigned long long)ent.stat.RegId(),
                (unsigned long long)ent.stat.SnapId(),
                (unsigned long long)ent.stat.InodeNo(), fname.c_str());
      }
    }
#endif

    return s;
  }

  virtual Status Fstat(const Slice& fentry, Handle* fh, uint64_t* mtime,
                       uint64_t* size, bool skip_cache = false) {
    Status s;
    int fd = reinterpret_cast<intptr_t>(fh);
    struct stat buf;
    int r = fstat(fd, &buf);
    if (r == 0) {
      *mtime = 1000ULL * 1000ULL * buf.st_mtime;
      *size = buf.st_size;
    } else {
      std::string fname = root_ + "/";
      fname += ToFileName(fentry);
      s = IOError(fname, errno);
    }

    return s;
  }

  virtual Status Write(const Slice& fentry, Handle* fh, const Slice& buf) {
    Status s;
    int fd = reinterpret_cast<intptr_t>(fh);
    ssize_t n = write(fd, buf.data(), buf.size());
    if (n == -1) {
      std::string fname = root_ + "/";
      fname += ToFileName(fentry);
      s = IOError(fname, errno);
    }

    return s;
  }

  virtual Status Pwrite(const Slice& fentry, Handle* fh, const Slice& buf,
                        uint64_t off) {
    Status s;
    int fd = reinterpret_cast<intptr_t>(fh);
    ssize_t n = pwrite(fd, buf.data(), buf.size(), off);
    if (n == -1) {
      std::string fname = root_ + "/";
      fname += ToFileName(fentry);
      s = IOError(fname, errno);
    }

    return s;
  }

  virtual Status Read(const Slice& fentry, Handle* fh, Slice* result,
                      uint64_t size, char* scratch) {
    Status s;
    int fd = reinterpret_cast<intptr_t>(fh);
    ssize_t n = read(fd, scratch, size);
    if (n == -1) {
      std::string fname = root_ + "/";
      fname += ToFileName(fentry);
      s = IOError(fname, errno);
    }
    if (s.ok()) {
      *result = Slice(scratch, n);
    }

    return s;
  }

  virtual Status Pread(const Slice& fentry, Handle* fh, Slice* result,
                       uint64_t off, uint64_t size, char* scratch) {
    Status s;
    int fd = reinterpret_cast<intptr_t>(fh);
    ssize_t n = pread(fd, scratch, size, off);
    if (n == -1) {
      std::string fname = root_ + "/";
      fname += ToFileName(fentry);
      s = IOError(fname, errno);
    }
    if (s.ok()) {
      *result = Slice(scratch, n);
    }

    return s;
  }

  virtual Status Ftruncate(const Slice& fentry, Handle* fh, uint64_t size) {
    Status s;
    int fd = reinterpret_cast<intptr_t>(fh);
    int r = ftruncate(fd, size);
    if (r != 0) {
      std::string fname = root_ + "/";
      fname += ToFileName(fentry);
      s = IOError(fname, errno);
    }

    return s;
  }

  virtual Status Flush(const Slice& fentry, Handle* fh,
                       bool force_sync = false) {
    Status s;
    if (force_sync) {
      int fd = reinterpret_cast<intptr_t>(fh);
      int r = fdatasync(fd);
      if (r != 0) {
        std::string fname = root_ + "/";
        fname += ToFileName(fentry);
        s = IOError(fname, errno);
      }
    }

    return s;
  }

  virtual Status Close(const Slice& fentry, Handle* fh) {
    close(reinterpret_cast<intptr_t>(fh));
    return Status::OK();
  }

  virtual Status Truncate(const Slice& fentry, uint64_t size) {
    Status s;
    std::string fname = root_ + "/";
    fname += ToFileName(fentry);
    int r = truncate(fname.c_str(), size);
    if (r != 0) {
      return IOError(fname, errno);
    } else {
      return s;
    }
  }

  virtual Status Stat(const Slice& fentry, uint64_t* mtime, uint64_t* size) {
    Status s;
    std::string fname = root_ + "/";
    fname += ToFileName(fentry);
    struct stat buf;
    int r = stat(fname.c_str(), &buf);
    if (r != 0) {
      return IOError(fname, errno);
    } else {
      *mtime = 1000ULL * 1000ULL * buf.st_mtime;
      *size = buf.st_size;
      return s;
    }
  }

  virtual Status Drop(const Slice& fentry) {
    Status s;
    std::string fname = root_ + "/";
    fname += ToFileName(fentry);
    int r = unlink(fname.c_str());
    if (r != 0) {
      return IOError(fname, errno);
    } else {
      return s;
    }
  }

 private:
  std::string root_;
};

}  // namespace pdlfs
