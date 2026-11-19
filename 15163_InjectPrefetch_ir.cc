// attr [compute(T_sigmoid, 0x2dd01260)] realize_scope = ""
realize T_sigmoid([0, 1], [0, 64], [0, 1], [0, 1]) {
  produce T_sigmoid {
    parallel (ax0.ax1.fused, 0, 64) {
      T_sigmoid((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0) =sigmoid(placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0))
    }
  }
}
