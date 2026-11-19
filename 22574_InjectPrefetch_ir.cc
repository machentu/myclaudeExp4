// attr [compute(T_reshape, 0x3ee31830)] realize_scope = ""
realize T_reshape([0, 1], [0, 65], [0, 120]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 65) {
      for (ax2, 0, 120) {
        T_reshape((ax0.ax1.fused/65), (ax0.ax1.fused % 65), ax2) =placeholder(0, ((((((((ax0.ax1.fused/65)*65) + (ax0.ax1.fused % 65))*120) + ax2)/15)/8) % 65), (((((((ax0.ax1.fused/65)*65) + (ax0.ax1.fused % 65))*120) + ax2)/15) % 8), ((((((ax0.ax1.fused/65)*65) + (ax0.ax1.fused % 65))*120) + ax2) % 15))
      }
    }
  }
}
