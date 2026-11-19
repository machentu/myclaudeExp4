// attr [compute(T_multiply, 0x3720b230)] realize_scope = ""
realize T_multiply([0, 4], [0, 16], [0, 1], [0, 2520]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax3, 0, 2520) {
        T_multiply((ax0.ax1.fused/16), (ax0.ax1.fused % 16), 0, ax3) =(placeholder((ax0.ax1.fused/16), (ax0.ax1.fused % 16), 0, ax3)*placeholder())
      }
    }
  }
}
