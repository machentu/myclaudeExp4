// attr [compute(T_relu, 0x376d32e0)] realize_scope = ""
realize T_relu([0, 1], [0, 80], [0, 8], [0, 15]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 80) {
      for (ax2, 0, 8) {
        for (ax3, 0, 15) {
          T_relu((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2, ax3) =max(placeholder((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2, ax3), (int8)0)
        }
      }
    }
  }
}
