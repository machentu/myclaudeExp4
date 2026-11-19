// attr [compute(T_concat, 0x43a59660)] realize_scope = ""
realize T_concat([0, 1], [0, 17], [0, 3], [0, 2520]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 17) {
      for (ax2, 0, 3) {
        for (ax3, 0, 2520) {
          T_concat((ax0.ax1.fused/17), (ax0.ax1.fused % 17), ax2, ax3) =tvm_if_then_else(((ax2 - 2) >= 0), placeholder((ax0.ax1.fused/17), (ax0.ax1.fused % 17), (ax2 - 2), ax3), placeholder((ax0.ax1.fused/17), (ax0.ax1.fused % 17), ax2, ax3))
        }
      }
    }
  }
}
