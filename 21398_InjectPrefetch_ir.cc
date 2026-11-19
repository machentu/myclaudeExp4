// attr [compute(T_multiply, 0x4dca6880)] realize_scope = ""
realize T_multiply([0, 1], [0, 32], [0, 64], [0, 120]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 64) {
        for (ax3, 0, 120) {
          T_multiply((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3) =(int64(placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3))*int64(placeholder((ax0.ax1.fused % 32), 0, 0)))
        }
      }
    }
  }
}
