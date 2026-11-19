// attr [compute(T_multiply, 0x47dbb850)] realize_scope = ""
realize T_multiply([0, 1], [0, 51], [0, 8], [0, 15]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 51) {
      for (ax2, 0, 8) {
        for (ax3, 0, 15) {
          T_multiply((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2, ax3) =(int64(placeholder((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2, ax3))*int64(placeholder((ax0.ax1.fused % 51), 0, 0)))
        }
      }
    }
  }
}
