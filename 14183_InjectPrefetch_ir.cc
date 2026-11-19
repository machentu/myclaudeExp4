// attr [compute(T_round, 0x256feae0)] realize_scope = ""
realize T_round([0, 1], [0, 96], [0, 16], [0, 8]) {
  produce T_round {
    parallel (ax0.ax1.fused, 0, 96) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_round((ax0.ax1.fused/96), (ax0.ax1.fused % 96), ax2, ax3) =round(placeholder((ax0.ax1.fused/96), (ax0.ax1.fused % 96), ax2, ax3))
        }
      }
    }
  }
}
