// attr [compute(T_reshape, 0x480983d0)] realize_scope = ""
realize T_reshape([0, 1], [0, 65], [0, 480]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 65) {
      for (ax2, 0, 480) {
        T_reshape((ax0.ax1.fused/65), (ax0.ax1.fused % 65), ax2) =placeholder(0, ((((((((ax0.ax1.fused/65)*65) + (ax0.ax1.fused % 65))*480) + ax2)/30)/16) % 65), (((((((ax0.ax1.fused/65)*65) + (ax0.ax1.fused % 65))*480) + ax2)/30) % 16), ((((((ax0.ax1.fused/65)*65) + (ax0.ax1.fused % 65))*480) + ax2) % 30))
      }
    }
  }
}
