// attr [compute(T_concat, 0x370a4d40)] realize_scope = ""
realize T_concat([0, 1], [0, 48], [0, 64], [0, 120]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 48) {
      for (ax2, 0, 64) {
        for (ax3, 0, 120) {
          T_concat((ax0.ax1.fused/48), (ax0.ax1.fused % 48), ax2, ax3) =tvm_if_then_else(((((ax0.ax1.fused % 48) - 16) - 16) >= 0), placeholder((ax0.ax1.fused/48), (((ax0.ax1.fused % 48) - 16) - 16), ax2, ax3), tvm_if_then_else((((ax0.ax1.fused % 48) - 16) >= 0), placeholder((ax0.ax1.fused/48), ((ax0.ax1.fused % 48) - 16), ax2, ax3), placeholder((ax0.ax1.fused/48), (ax0.ax1.fused % 48), ax2, ax3)))
        }
      }
    }
  }
}
