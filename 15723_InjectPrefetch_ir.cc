// attr [compute(T_sigmoid, 0x2dbe6080)] realize_scope = ""
realize T_sigmoid([0, 1], [0, 128], [0, 1], [0, 1]) {
  produce T_sigmoid {
    parallel (ax0.ax1.fused, 0, 128) {
      T_sigmoid((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0) =sigmoid(placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0))
    }
  }
}
