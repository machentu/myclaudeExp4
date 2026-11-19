// attr [compute(T_cast, 0x2dbecd50)] realize_scope = ""
realize T_cast([0, 1], [0, 512], [0, 8], [0, 4]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 512) {
      for (ax2, 0, 8) {
        for (ax3, 0, 4) {
          T_cast((ax0.ax1.fused/512), (ax0.ax1.fused % 512), ax2, ax3) =float16(placeholder((ax0.ax1.fused/512), (ax0.ax1.fused % 512), ax2, ax3))
        }
      }
    }
  }
}
