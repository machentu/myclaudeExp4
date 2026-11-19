// attr [compute(T_multiply, 0x22ad4480)] realize_scope = ""
realize T_multiply([0, 1], [0, 8], [0, 1], [0, 1]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 8) {
      T_multiply((ax0.ax1.fused/8), (ax0.ax1.fused % 8), 0, 0) =(int64(placeholder((ax0.ax1.fused/8), (ax0.ax1.fused % 8), 0, 0))*int64(placeholder((ax0.ax1.fused % 8), 0, 0)))
    }
  }
}
