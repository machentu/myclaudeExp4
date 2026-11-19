// attr [compute(T_round, 0x2e706280)] realize_scope = ""
realize T_round([0, 1], [0, 512], [0, 8], [0, 4]) {
  produce T_round {
    parallel (ax0.ax1.fused, 0, 512) {
      for (ax2, 0, 8) {
        for (ax3, 0, 4) {
          T_round((ax0.ax1.fused/512), (ax0.ax1.fused % 512), ax2, ax3) =round(placeholder((ax0.ax1.fused/512), (ax0.ax1.fused % 512), ax2, ax3))
        }
      }
    }
  }
}
