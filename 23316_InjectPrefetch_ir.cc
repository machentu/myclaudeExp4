// attr [compute(T_reshape, 0x3ee883c0)] realize_scope = ""
realize T_reshape([0, 1], [0, 51], [0, 480]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 51) {
      for (ax2, 0, 480) {
        T_reshape((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2) =placeholder(0, ((((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*480) + ax2)/30)/16) % 51), (((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*480) + ax2)/30) % 16), ((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*480) + ax2) % 30))
      }
    }
  }
}
