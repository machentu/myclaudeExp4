// attr [compute(T_cast, 0x2a6eafc0)] realize_scope = ""
realize T_cast([0, 1], [0, 96], [0, 16], [0, 8]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 96) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_cast((ax0.ax1.fused/96), (ax0.ax1.fused % 96), ax2, ax3) =float16(placeholder((ax0.ax1.fused/96), (ax0.ax1.fused % 96), ax2, ax3))
        }
      }
    }
  }
}
