// attr [compute(T_cast, 0x310fb660)] realize_scope = ""
realize T_cast([0, 1], [0, 256], [0, 32], [0, 16]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 256) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          T_cast((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) =float16(placeholder((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3))
        }
      }
    }
  }
}
