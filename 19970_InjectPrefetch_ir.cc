// attr [compute(T_round, 0x4c5991b0)] realize_scope = ""
realize T_round([0, 4], [0, 16], [0, 1], [0, 2520]) {
  produce T_round {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax3, 0, 2520) {
        T_round((ax0.ax1.fused/16), (ax0.ax1.fused % 16), 0, ax3) =round(placeholder((ax0.ax1.fused/16), (ax0.ax1.fused % 16), 0, ax3))
      }
    }
  }
}
