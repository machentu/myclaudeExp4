// attr [compute(T_multiply, 0x362f66d0)] realize_scope = ""
realize T_multiply([0, 1], [0, 144], [0, 2520]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 144) {
      for (ax2, 0, 2520) {
        T_multiply((ax0.ax1.fused/144), (ax0.ax1.fused % 144), ax2) =(int64(placeholder((ax0.ax1.fused/144), (ax0.ax1.fused % 144), ax2))*int64(placeholder()))
      }
    }
  }
}
