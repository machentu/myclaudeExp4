// attr [compute(T_relu, 0x2e239310)] realize_scope = ""
realize T_relu([0, 1], [0, 4], [0, 1], [0, 1]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 4) {
      T_relu((ax0.ax1.fused/4), (ax0.ax1.fused % 4), 0, 0) =max(placeholder((ax0.ax1.fused/4), (ax0.ax1.fused % 4), 0, 0), (int8)0)
    }
  }
}
