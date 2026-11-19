// attr [compute(T_relu, 0x39043cc0)] realize_scope = ""
realize T_relu([0, 1], [0, 32], [0, 16], [0, 30]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_relu((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3) =max(placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3), (int8)0)
        }
      }
    }
  }
}
