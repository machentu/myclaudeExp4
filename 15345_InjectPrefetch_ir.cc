// attr [compute(T_multiply, 0x1b450a00)] realize_scope = ""
realize T_multiply([0, 1], [0, 4], [0, 1], [0, 1]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 4) {
      T_multiply((ax0.ax1.fused/4), (ax0.ax1.fused % 4), 0, 0) =(int64(placeholder((ax0.ax1.fused/4), (ax0.ax1.fused % 4), 0, 0))*int64(placeholder((ax0.ax1.fused % 4), 0, 0)))
    }
  }
}
