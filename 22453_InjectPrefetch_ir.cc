// attr [compute(T_multiply, 0x36380470)] realize_scope = ""
realize T_multiply([0, 1], [0, 32], [0, 2520]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 2520) {
        T_multiply((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2) =(placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2)*placeholder())
      }
    }
  }
}
