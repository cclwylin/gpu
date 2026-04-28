#include "gpu_systemc/gpu_top_ca.h"

namespace gpu::systemc {

GpuTopCa::GpuTopCa(sc_core::sc_module_name name)
    : sc_module(name),
      clk("clk"), rst_n("rst_n"),
      out_valid_o("out_valid_o"),
      out_ready_i("out_ready_i"),
      out_data_o ("out_data_o"),
      cp("cp"), vf("vf"), sc("sc"),
      cp_vf_valid_("cp_vf_valid"), cp_vf_ready_("cp_vf_ready"),
      cp_vf_data_ ("cp_vf_data"),
      vf_sc_valid_("vf_sc_valid"), vf_sc_ready_("vf_sc_ready"),
      vf_sc_data_ ("vf_sc_data") {
    cp.clk(clk); cp.rst_n(rst_n);
    cp.cmd_valid_o(cp_vf_valid_);
    cp.cmd_ready_i(cp_vf_ready_);
    cp.cmd_data_o (cp_vf_data_);

    vf.clk(clk); vf.rst_n(rst_n);
    vf.cmd_valid_i(cp_vf_valid_);
    vf.cmd_ready_o(cp_vf_ready_);
    vf.cmd_data_i (cp_vf_data_);
    vf.vert_valid_o(vf_sc_valid_);
    vf.vert_ready_i(vf_sc_ready_);
    vf.vert_data_o (vf_sc_data_);

    sc.clk(clk); sc.rst_n(rst_n);
    sc.job_valid_i(vf_sc_valid_);
    sc.job_ready_o(vf_sc_ready_);
    sc.job_data_i (vf_sc_data_);
    sc.out_valid_o(out_valid_o);
    sc.out_ready_i(out_ready_i);
    sc.out_data_o (out_data_o);
}

}  // namespace gpu::systemc
