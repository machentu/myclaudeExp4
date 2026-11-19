// attr [compute(T_reshape, 0x39319960)] realize_scope = ""
realize T_reshape([0, 1], [0, 32], [0, 480]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 480) {
        T_reshape((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2) =placeholder(0, ((((((((ax0.ax1.fused/32)*32) + (ax0.ax1.fused % 32))*480) + ax2)/30)/16) % 32), (((((((ax0.ax1.fused/32)*32) + (ax0.ax1.fused % 32))*480) + ax2)/30) % 16), ((((((ax0.ax1.fused/32)*32) + (ax0.ax1.fused % 32))*480) + ax2) % 30))
      }
    }
  }
}
