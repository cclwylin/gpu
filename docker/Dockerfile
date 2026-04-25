# =============================================================================
# GPU project reproducible build environment
# =============================================================================
#
# Build:
#   docker build -f docker/Dockerfile -t gpu-dev:v1 .
#
# Run:
#   docker run --rm -it -v $PWD:/work -w /work gpu-dev:v1 bash
#
# 版本對齊 third_party/versions.yaml 的 toolchain 區塊。
# -----------------------------------------------------------------------------

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

# -----------------------------------------------------------------------------
# Base toolchain
# -----------------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      gcc-11 g++-11 \
      clang-14 clang-format-14 clang-tidy-14 \
      cmake=3.22.1-1ubuntu1.22.04.2 \
      ninja-build \
      python3.10 python3-pip python3-venv \
      git ca-certificates curl wget \
      pkg-config \
      zlib1g-dev libssl-dev \
      flex bison \
      autoconf automake libtool \
      tcl-dev \
      graphviz \
      ccache \
   && rm -rf /var/lib/apt/lists/* \
   && update-alternatives --install /usr/bin/python python /usr/bin/python3.10 1

# -----------------------------------------------------------------------------
# Verilator 5.020
# -----------------------------------------------------------------------------
ARG VERILATOR_REV=v5.020
RUN git clone --depth 1 --branch $VERILATOR_REV \
      https://github.com/verilator/verilator.git /tmp/verilator \
 && cd /tmp/verilator && autoconf && ./configure && make -j"$(nproc)" && make install \
 && rm -rf /tmp/verilator

# -----------------------------------------------------------------------------
# Icarus 12.0
# -----------------------------------------------------------------------------
ARG ICARUS_REV=v12_0
RUN git clone --depth 1 --branch $ICARUS_REV \
      https://github.com/steveicarus/iverilog.git /tmp/iverilog \
 && cd /tmp/iverilog && sh autoconf.sh && ./configure && make -j"$(nproc)" && make install \
 && rm -rf /tmp/iverilog

# -----------------------------------------------------------------------------
# Yosys 0.38 + SymbiYosys
# -----------------------------------------------------------------------------
ARG YOSYS_REV=yosys-0.38
RUN git clone --depth 1 --branch $YOSYS_REV \
      https://github.com/YosysHQ/yosys.git /tmp/yosys \
 && cd /tmp/yosys && make config-clang && make -j"$(nproc)" && make install \
 && rm -rf /tmp/yosys

RUN git clone --depth 1 https://github.com/YosysHQ/sby.git /tmp/sby \
 && cd /tmp/sby && make install \
 && rm -rf /tmp/sby

# -----------------------------------------------------------------------------
# SystemC 2.3.3
# -----------------------------------------------------------------------------
ARG SYSTEMC_REV=v2.3.3
ENV SYSTEMC_HOME=/opt/systemc
RUN git clone --depth 1 --branch $SYSTEMC_REV \
      https://github.com/accellera-official/systemc.git /tmp/systemc \
 && cd /tmp/systemc && mkdir build && cd build \
 && cmake -DCMAKE_INSTALL_PREFIX=$SYSTEMC_HOME -DCMAKE_CXX_STANDARD=17 .. \
 && make -j"$(nproc)" && make install \
 && rm -rf /tmp/systemc
ENV LD_LIBRARY_PATH=$SYSTEMC_HOME/lib:$LD_LIBRARY_PATH

# -----------------------------------------------------------------------------
# UVM-SystemC 1.0-beta5  (tarball)
# -----------------------------------------------------------------------------
ENV UVM_SYSTEMC_HOME=/opt/uvm-systemc
RUN wget -q https://www.accellera.org/images/downloads/standards/systemc/uvm-systemc-1.0-beta5.tar.gz -O /tmp/uvm.tgz \
 && mkdir -p /tmp/uvm && tar xzf /tmp/uvm.tgz -C /tmp/uvm --strip-components=1 \
 && cd /tmp/uvm && ./configure --prefix=$UVM_SYSTEMC_HOME --with-systemc=$SYSTEMC_HOME \
 && make -j"$(nproc)" && make install \
 && rm -rf /tmp/uvm /tmp/uvm.tgz
ENV LD_LIBRARY_PATH=$UVM_SYSTEMC_HOME/lib:$LD_LIBRARY_PATH

# -----------------------------------------------------------------------------
# DRAMsim3
# -----------------------------------------------------------------------------
ENV DRAMSIM3_HOME=/opt/DRAMsim3
RUN git clone https://github.com/umd-memsys/DRAMsim3.git $DRAMSIM3_HOME \
 && cd $DRAMSIM3_HOME && mkdir build && cd build \
 && cmake .. && make -j"$(nproc)"

# -----------------------------------------------------------------------------
# glslang 14.0.0
# -----------------------------------------------------------------------------
ARG GLSLANG_REV=14.0.0
RUN git clone --depth 1 --branch $GLSLANG_REV \
      https://github.com/KhronosGroup/glslang.git /opt/glslang \
 && cd /opt/glslang && ./update_glslang_sources.py \
 && mkdir build && cd build \
 && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local .. \
 && make -j"$(nproc)" && make install

# -----------------------------------------------------------------------------
# Python deps
# -----------------------------------------------------------------------------
COPY third_party/requirements.txt /tmp/requirements.txt
RUN pip3 install --no-cache-dir -r /tmp/requirements.txt

# -----------------------------------------------------------------------------
# User + workspace
# -----------------------------------------------------------------------------
ARG UID=1000
ARG GID=1000
RUN groupadd -g $GID dev && useradd -m -u $UID -g $GID -s /bin/bash dev
USER dev
WORKDIR /work

ENV PATH=/usr/local/bin:$PATH

CMD ["bash"]
