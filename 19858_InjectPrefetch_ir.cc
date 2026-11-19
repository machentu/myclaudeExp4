// attr [compute(T_reshape, 0x4cc78970)] realize_scope = ""
realize T_reshape([0, 1], [0, 4], [0, 2520]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 4) {
      for (ax2, 0, 2520) {
        T_reshape((ax0.ax1.fused/4), (ax0.ax1.fused % 4), ax2) =placeholder((((((((ax0.ax1.fused/4)*4) + (ax0.ax1.fused % 4))*2520) + ax2)/2520) % 4), 0, 0, ((((((ax0.ax1.fused/4)*4) + (ax0.ax1.fused % 4))*2520) + ax2) % 2520))
      }
    }
  }
}
