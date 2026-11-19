// attr [compute(T_multiply, 0x1ee87d50)] realize_scope = ""
realize T_multiply([0, 1], [0, 128], [0, 1], [0, 1]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 128) {
      T_multiply((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0) =(placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0)*placeholder())
    }
  }
}
