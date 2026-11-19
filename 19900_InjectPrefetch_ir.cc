// attr [compute(T_multiply, 0x471e6d90)] realize_scope = ""
realize T_multiply([0, 4], [0, 1], [0, 1], [0, 2520]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 4) {
      for (ax3, 0, 2520) {
        T_multiply(ax0.ax1.fused, 0, 0, ax3) =(int64(placeholder(ax0.ax1.fused, 0, 0, ax3))*int64(placeholder()))
      }
    }
  }
}
