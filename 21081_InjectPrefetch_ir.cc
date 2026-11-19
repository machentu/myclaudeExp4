// attr [compute(T_multiply, 0x35d8e270)] realize_scope = ""
realize T_multiply([0, 1], [0, 16], [0, 64], [0, 120]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 16) {
      for (ax2, 0, 64) {
        for (ax3, 0, 120) {
          T_multiply((ax0.ax1.fused/16), (ax0.ax1.fused % 16), ax2, ax3) =(int64(placeholder((ax0.ax1.fused/16), (ax0.ax1.fused % 16), ax2, ax3))*int64(placeholder((ax0.ax1.fused % 16), 0, 0)))
        }
      }
    }
  }
}
