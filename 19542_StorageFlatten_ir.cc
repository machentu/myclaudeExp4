produce T_concat {
  parallel (ax0.ax1.fused, 0, 144) {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_concat[((((ax0.ax1.fused*32) + ax2)*60) + ax3)] = tvm_if_then_else((((ax0.ax1.fused % 144) - 64) >= 0), placeholder[((((((((ax0.ax1.fused/144)*80) + (ax0.ax1.fused % 144))*32) + ax2)*60) + ax3) + -122880)], placeholder[(((((((ax0.ax1.fused/144)*64) + (ax0.ax1.fused % 144))*32) + ax2)*60) + ax3)])
      }
    }
  }
}
