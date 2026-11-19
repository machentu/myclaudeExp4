// attr [compute(T_relu, 0x1c165730)] realize_scope = ""
realize T_relu([0, 1], [0, 96], [0, 16], [0, 8]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 96) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_relu((ax0.ax1.fused/96), (ax0.ax1.fused % 96), ax2, ax3) =max(placeholder((ax0.ax1.fused/96), (ax0.ax1.fused % 96), ax2, ax3), (int8)0)
        }
      }
    }
  }
}
