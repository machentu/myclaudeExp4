// attr [compute(T_relu, 0x297ba740)] realize_scope = ""
realize T_relu([0, 1], [0, 6], [0, 1], [0, 1]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 6) {
      T_relu((ax0.ax1.fused/6), (ax0.ax1.fused % 6), 0, 0) =max(placeholder((ax0.ax1.fused/6), (ax0.ax1.fused % 6), 0, 0), (int8)0)
    }
  }
}
