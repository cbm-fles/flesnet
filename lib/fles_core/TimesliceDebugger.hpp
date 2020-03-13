// Copyright 2013-2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "MicrosliceDescriptor.hpp"
#include "Sink.hpp"
#include "Timeslice.hpp"
#include <ostream>

class BufferDump {
public:
  BufferDump(const void* arg_buf, std::size_t arg_size)
      : buf(arg_buf), size(arg_size) {}
  friend std::ostream& operator<<(std::ostream& s, const BufferDump& dump);
  std::ostream& write_to_stream(std::ostream& s) const;

private:
  const void* buf;
  std::size_t size;
};

inline std::ostream& operator<<(std::ostream& s, const BufferDump& dump) {
  return dump.write_to_stream(s);
}

// ----------

class MicrosliceDescriptorDump {
public:
  explicit MicrosliceDescriptorDump(const fles::MicrosliceDescriptor& arg_md)
      : md(arg_md) {}
  friend std::ostream& operator<<(std::ostream& s,
                                  const MicrosliceDescriptorDump& dump);
  std::ostream& write_to_stream(std::ostream& s) const;

private:
  const fles::MicrosliceDescriptor& md;
};

inline std::ostream& operator<<(std::ostream& s,
                                const MicrosliceDescriptorDump& dump) {
  return dump.write_to_stream(s);
}

inline std::ostream& operator<<(std::ostream& s,
                                const fles::MicrosliceView& m) {
  return s << MicrosliceDescriptorDump(m.desc()) << "\n"
           << BufferDump(m.content(), m.desc().size);
}

// ----------

class MicrosliceDumper : public fles::MicrosliceSink {
public:
  MicrosliceDumper(std::ostream& arg_out, std::size_t arg_verbosity)
      : out(arg_out), verbosity(arg_verbosity){};

  void put(std::shared_ptr<const fles::Microslice> m) override {
    out << MicrosliceDescriptorDump(m->desc()) << "\n";
    if (verbosity > 1) {
      out << BufferDump(m->content(), m->desc().size);
    }
  }

private:
  std::ostream& out;
  std::size_t verbosity;
};

// ----------

class TimesliceDump {
public:
  TimesliceDump(const fles::Timeslice& arg_ts, std::size_t arg_verbosity)
      : ts(arg_ts), verbosity(arg_verbosity) {}
  friend std::ostream& operator<<(std::ostream& s, const TimesliceDump& dump);
  std::ostream& write_to_stream(std::ostream& s) const;

private:
  const fles::Timeslice& ts;
  std::size_t verbosity;
};

inline std::ostream& operator<<(std::ostream& s, const TimesliceDump& dump) {
  return dump.write_to_stream(s);
}

// ----------

class TimesliceDumper : public fles::TimesliceSink {
public:
  TimesliceDumper(std::ostream& arg_out, std::size_t arg_verbosity)
      : out(arg_out), verbosity(arg_verbosity){};

  void put(std::shared_ptr<const fles::Timeslice> timeslice) override {
    out << TimesliceDump(*timeslice, verbosity) << "\n";
  }

private:
  std::ostream& out;
  std::size_t verbosity;
};
