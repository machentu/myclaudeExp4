// attr [compute(T_cast, 0x29c75040)] realize_scope = ""
realize T_cast([0, 1], [0, 64], [0, 32], [0, 16]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          T_cast((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3) =float16(placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3))
        }
      }
    }
  }
}
