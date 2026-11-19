// attr [compute(T_reshape, 0x4ad32d70)] realize_scope = ""
realize T_reshape([0, 1], [0, 51], [0, 120]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 51) {
      for (ax2, 0, 120) {
        T_reshape((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2) =placeholder(0, ((((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*120) + ax2)/15)/8) % 51), (((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*120) + ax2)/15) % 8), ((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*120) + ax2) % 15))
      }
    }
  }
}
