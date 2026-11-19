// attr [compute(T_relu, 0x2e846680)] realize_scope = ""
realize T_relu([0, 1], [0, 384], [0, 16], [0, 8]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 384) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_relu((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =max(placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3), (int8)0)
        }
      }
    }
  }
}
