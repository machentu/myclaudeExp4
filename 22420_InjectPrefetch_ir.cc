// attr [compute(T_concat, 0x47aed4f0)] realize_scope = ""
realize T_concat([0, 1], [0, 65], [0, 16], [0, 30]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 65) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_concat((ax0.ax1.fused/65), (ax0.ax1.fused % 65), ax2, ax3) =tvm_if_then_else((((ax0.ax1.fused % 65) - 64) >= 0), placeholder((ax0.ax1.fused/65), ((ax0.ax1.fused % 65) - 64), ax2, ax3), placeholder((ax0.ax1.fused/65), (ax0.ax1.fused % 65), ax2, ax3))
        }
      }
    }
  }
}
