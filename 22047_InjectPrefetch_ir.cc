// attr [compute(T_multiply, 0x3c4120f0)] realize_scope = ""
realize T_multiply([0, 1], [0, 144], [0, 120]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 144) {
      for (ax2, 0, 120) {
        T_multiply((ax0.ax1.fused/144), (ax0.ax1.fused % 144), ax2) =(int32(placeholder((ax0.ax1.fused/144), (ax0.ax1.fused % 144), ax2))*int32(placeholder()))
      }
    }
  }
}
