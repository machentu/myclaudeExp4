// attr [compute(T_reshape, 0x38f52d40)] realize_scope = ""
realize T_reshape([0, 1], [0, 144], [0, 480]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 144) {
      for (ax2, 0, 480) {
        T_reshape((ax0.ax1.fused/144), (ax0.ax1.fused % 144), ax2) =placeholder(0, ((((((((ax0.ax1.fused/144)*144) + (ax0.ax1.fused % 144))*480) + ax2)/30)/16) % 144), (((((((ax0.ax1.fused/144)*144) + (ax0.ax1.fused % 144))*480) + ax2)/30) % 16), ((((((ax0.ax1.fused/144)*144) + (ax0.ax1.fused % 144))*480) + ax2) % 30))
      }
    }
  }
}
