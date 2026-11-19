// attr [compute(T_relu, 0x2d7ba560)] realize_scope = ""
realize T_relu([0, 1], [0, 256], [0, 32], [0, 16]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 256) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          T_relu((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) =max(placeholder((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3), (int8)0)
        }
      }
    }
  }
}
