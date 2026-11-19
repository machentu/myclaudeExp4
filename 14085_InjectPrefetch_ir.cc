// attr [compute(T_round, 0x1562c7d0)] realize_scope = ""
realize T_round([0, 1], [0, 384], [0, 16], [0, 8]) {
  produce T_round {
    parallel (ax0.ax1.fused, 0, 384) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_round((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =round(placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3))
        }
      }
    }
  }
}
