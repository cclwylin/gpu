#pragma once
#include <cstdint>
#include <fstream>
#include <string>

namespace gpu::trace {

// Stage-boundary trace writer. Each pipeline stage can dump structured
// records into a sink for later HW co-sim diff.
//
// Format (skeleton, v0): one record per line, JSON-ish text.
//   {"stage":"VS","tid":42,"out":[...]}
// Phase 1 may switch to a binary length-prefixed format if perf demands.

class Sink {
public:
    explicit Sink(const std::string& path);
    ~Sink();
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;

    void event(const std::string& stage, const std::string& payload);

    bool ok() const { return ofs_.is_open(); }

private:
    std::ofstream ofs_;
};

}  // namespace gpu::trace
