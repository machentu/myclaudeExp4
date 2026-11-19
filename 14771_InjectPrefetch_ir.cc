// attr [compute(T_round, 0x1c189d20)] realize_scope = ""
realize T_round([0, 1], [0, 64], [0, 32], [0, 16]) {
  produce T_round {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          T_round((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3) =round(placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3))
        }
      }
    }
  }
}
