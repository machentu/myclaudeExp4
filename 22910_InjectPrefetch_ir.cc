// attr [compute(T_reshape, 0x48aa34b0)] realize_scope = ""
realize T_reshape([0, 1], [0, 51], [0, 2520]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 51) {
      for (ax2, 0, 2520) {
        T_reshape((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2) =placeholder(0, ((((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*2520) + ax2)/2520)/3) % 17), (((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*2520) + ax2)/2520) % 3), ((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*2520) + ax2) % 2520))
      }
    }
  }
}
