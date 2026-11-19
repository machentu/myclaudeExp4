// attr [compute(T_multiply, 0x1c0c8e20)] realize_scope = ""
realize T_multiply([0, 1], [0, 96], [0, 1], [0, 1]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 96) {
      T_multiply((ax0.ax1.fused/96), (ax0.ax1.fused % 96), 0, 0) =(int64(placeholder((ax0.ax1.fused/96), (ax0.ax1.fused % 96), 0, 0))*int64(placeholder((ax0.ax1.fused % 96), 0, 0)))
    }
  }
}
