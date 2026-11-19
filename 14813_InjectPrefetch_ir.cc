// attr [compute(T_multiply, 0x14589180)] realize_scope = ""
realize T_multiply([0, 1], [0, 64], [0, 32], [0, 16]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          T_multiply((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3) =(placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3)*placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0))
        }
      }
    }
  }
}
