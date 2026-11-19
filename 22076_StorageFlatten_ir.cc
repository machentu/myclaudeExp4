produce T_concat {
  parallel (ax0.ax1.fused, 0, 144) {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_concat[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] = tvm_if_then_else((((ax0.ax1.fused % 144) - 64) >= 0), placeholder[((((((((ax0.ax1.fused/144)*80) + (ax0.ax1.fused % 144))*8) + ax2)*15) + ax3) + -7680)], placeholder[(((((((ax0.ax1.fused/144)*64) + (ax0.ax1.fused % 144))*8) + ax2)*15) + ax3)])
      }
    }
  }
}
