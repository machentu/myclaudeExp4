// attr [compute(T_reshape, 0x47cbc390)] realize_scope = ""
realize T_reshape([0, 4], [0, 16], [0, 1], [0, 2520]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax3, 0, 2520) {
        T_reshape((ax0.ax1.fused/16), (ax0.ax1.fused % 16), 0, ax3) =placeholder(0, ((((((((ax0.ax1.fused/16)*16) + (ax0.ax1.fused % 16)) + 0)*2520) + ax3)/2520) % 64), (((((((ax0.ax1.fused/16)*16) + (ax0.ax1.fused % 16)) + 0)*2520) + ax3) % 2520))
      }
    }
  }
}
