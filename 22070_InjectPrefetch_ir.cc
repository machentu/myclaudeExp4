// attr [compute(T_relu, 0x3f1fffd0)] realize_scope = ""
realize T_relu([0, 1], [0, 64], [0, 16], [0, 30]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_relu((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3) =max(placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3), (int8)0)
        }
      }
    }
  }
}
