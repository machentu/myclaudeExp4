// attr [compute(T_multiply, 0x48a2d5c0)] realize_scope = ""
realize T_multiply([0, 1], [0, 4], [0, 2520]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 4) {
      for (ax2, 0, 2520) {
        T_multiply((ax0.ax1.fused/4), (ax0.ax1.fused % 4), ax2) =(placeholder((ax0.ax1.fused/4), (ax0.ax1.fused % 4), ax2)*placeholder())
      }
    }
  }
}
