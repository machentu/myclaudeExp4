// attr [compute(T_multiply, 0x1e3f1f60)] realize_scope = ""
realize T_multiply([0, 1], [0, 64], [0, 1], [0, 1]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 64) {
      T_multiply((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0) =(int64(placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0))*int64(placeholder((ax0.ax1.fused % 64), 0, 0)))
    }
  }
}
