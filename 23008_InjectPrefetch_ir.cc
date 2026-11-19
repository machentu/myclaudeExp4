// attr [compute(T_reshape, 0x47b2bad0)] realize_scope = ""
realize T_reshape([0, 1], [0, 17], [0, 3], [0, 2520]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 17) {
      for (ax2, 0, 3) {
        for (ax3, 0, 2520) {
          T_reshape((ax0.ax1.fused/17), (ax0.ax1.fused % 17), ax2, ax3) =placeholder(0, (((((((((ax0.ax1.fused/17)*17) + (ax0.ax1.fused % 17))*3) + ax2)*2520) + ax3)/2520) % 51), ((((((((ax0.ax1.fused/17)*17) + (ax0.ax1.fused % 17))*3) + ax2)*2520) + ax3) % 2520))
        }
      }
    }
  }
}
