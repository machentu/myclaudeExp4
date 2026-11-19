// attr [compute(T_relu, 0x3115fda0)] realize_scope = ""
realize T_relu([0, 1], [0, 8], [0, 1], [0, 1]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 8) {
      T_relu((ax0.ax1.fused/8), (ax0.ax1.fused % 8), 0, 0) =max(placeholder((ax0.ax1.fused/8), (ax0.ax1.fused % 8), 0, 0), (int8)0)
    }
  }
}
