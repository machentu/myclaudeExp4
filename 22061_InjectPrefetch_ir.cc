// attr [compute(T_reshape, 0x35f6f7e0)] realize_scope = ""
realize T_reshape([0, 1], [0, 144], [0, 120]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 144) {
      for (ax2, 0, 120) {
        T_reshape((ax0.ax1.fused/144), (ax0.ax1.fused % 144), ax2) =placeholder(0, ((((((((ax0.ax1.fused/144)*144) + (ax0.ax1.fused % 144))*120) + ax2)/15)/8) % 144), (((((((ax0.ax1.fused/144)*144) + (ax0.ax1.fused % 144))*120) + ax2)/15) % 8), ((((((ax0.ax1.fused/144)*144) + (ax0.ax1.fused % 144))*120) + ax2) % 15))
      }
    }
  }
}
