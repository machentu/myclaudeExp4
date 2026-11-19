// attr [compute(T_cast, 0x31c85bf0)] realize_scope = ""
realize T_cast([0, 1], [0, 32], [0, 64], [0, 120]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 64) {
        for (ax3, 0, 120) {
          T_cast((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3) =float32(placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3))
        }
      }
    }
  }
}
