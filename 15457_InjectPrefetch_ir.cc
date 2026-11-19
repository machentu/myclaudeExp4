// attr [compute(T_sigmoid, 0x1c7ac2b0)] realize_scope = ""
realize T_sigmoid([0, 1], [0, 96], [0, 1], [0, 1]) {
  produce T_sigmoid {
    parallel (ax0.ax1.fused, 0, 96) {
      T_sigmoid((ax0.ax1.fused/96), (ax0.ax1.fused % 96), 0, 0) =sigmoid(placeholder((ax0.ax1.fused/96), (ax0.ax1.fused % 96), 0, 0))
    }
  }
}
