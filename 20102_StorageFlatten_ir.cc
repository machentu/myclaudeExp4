produce T_concat {
  parallel (ax0.ax1.fused, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_concat[((((ax0.ax1.fused*16) + ax2)*30) + ax3)] = tvm_if_then_else((((ax0.ax1.fused % 384) - 256) >= 0), placeholder[((((((((ax0.ax1.fused/384)*128) + (ax0.ax1.fused % 384))*16) + ax2)*30) + ax3) + -122880)], placeholder[(((((((ax0.ax1.fused/384)*256) + (ax0.ax1.fused % 384))*16) + ax2)*30) + ax3)])
      }
    }
  }
}
