// attr [compute(T_multiply, 0x2dd0e670)] realize_scope = ""
realize T_multiply([0, 1], [0, 512], [0, 1]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 512) {
      T_multiply((ax0.ax1.fused/512), (ax0.ax1.fused % 512), 0) =(int64(placeholder((ax0.ax1.fused/512), (ax0.ax1.fused % 512), 0))*int64(placeholder((ax0.ax1.fused % 512), 0)))
    }
  }
}
