// attr [compute(T_reshape, 0x3910f630)] realize_scope = ""
realize T_reshape([0, 1], [0, 32], [0, 120]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 120) {
        T_reshape((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2) =placeholder(0, ((((((((ax0.ax1.fused/32)*32) + (ax0.ax1.fused % 32))*120) + ax2)/15)/8) % 32), (((((((ax0.ax1.fused/32)*32) + (ax0.ax1.fused % 32))*120) + ax2)/15) % 8), ((((((ax0.ax1.fused/32)*32) + (ax0.ax1.fused % 32))*120) + ax2) % 15))
      }
    }
  }
}
