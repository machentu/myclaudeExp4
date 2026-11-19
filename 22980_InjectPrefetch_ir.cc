// attr [compute(T_cast, 0x40e500a0)] realize_scope = ""
realize T_cast([0, 1], [0, 17], [0, 2], [0, 2520]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 17) {
      for (ax2, 0, 2) {
        for (ax3, 0, 2520) {
          T_cast((ax0.ax1.fused/17), (ax0.ax1.fused % 17), ax2, ax3) =float16(placeholder((ax0.ax1.fused/17), (ax0.ax1.fused % 17), ax2, ax3))
        }
      }
    }
  }
}
