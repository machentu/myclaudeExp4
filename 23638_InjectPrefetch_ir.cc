// attr [compute(T_multiply, 0x4956df90)] realize_scope = ""
realize T_multiply([0, 1], [0, 17], [0, 1], [0, 2520]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 17) {
      for (ax3, 0, 2520) {
        T_multiply((ax0.ax1.fused/17), (ax0.ax1.fused % 17), 0, ax3) =(placeholder((ax0.ax1.fused/17), (ax0.ax1.fused % 17), 0, ax3)*placeholder())
      }
    }
  }
}
