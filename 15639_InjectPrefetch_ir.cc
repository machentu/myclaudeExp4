// attr [compute(T_multiply, 0x1c10d9f0)] realize_scope = ""
realize T_multiply([0, 1], [0, 6], [0, 1], [0, 1]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 6) {
      T_multiply((ax0.ax1.fused/6), (ax0.ax1.fused % 6), 0, 0) =(int64(placeholder((ax0.ax1.fused/6), (ax0.ax1.fused % 6), 0, 0))*int64(placeholder((ax0.ax1.fused % 6), 0, 0)))
    }
  }
}
