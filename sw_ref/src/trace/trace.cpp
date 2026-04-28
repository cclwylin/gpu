#include "gpu/trace.h"

namespace gpu::trace {

Sink::Sink(const std::string& path) : ofs_(path, std::ios::out | std::ios::trunc) {}
Sink::~Sink() { ofs_.close(); }

void Sink::event(const std::string& stage, const std::string& payload) {
    if (!ofs_) return;
    ofs_ << R"({"stage":")" << stage << R"(","payload":)" << payload << "}\n";
}

}  // namespace gpu::trace
