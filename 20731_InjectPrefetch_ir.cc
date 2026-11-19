// attr [compute(T_relu, 0x37e13280)] realize_scope = ""
realize T_relu([0, 1], [0, 32], [0, 64], [0, 120]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 64) {
        for (ax3, 0, 120) {
          T_relu((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3) =max(placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3), (int8)0)
        }
      }
    }
  }
}
