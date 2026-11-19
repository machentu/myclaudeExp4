produce T_concat {
  parallel (ax0.ax1.fused, 0, 512) {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_concat[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] = tvm_if_then_else((((((ax0.ax1.fused % 512) - 128) - 128) - 128) >= 0), placeholder[((((((((ax0.ax1.fused/512)*128) + (ax0.ax1.fused % 512))*8) + ax2)*15) + ax3) + -46080)], tvm_if_then_else(((((ax0.ax1.fused % 512) - 128) - 128) >= 0), placeholder[((((((((ax0.ax1.fused/512)*128) + (ax0.ax1.fused % 512))*8) + ax2)*15) + ax3) + -30720)], tvm_if_then_else((((ax0.ax1.fused % 512) - 128) >= 0), placeholder[((((((((ax0.ax1.fused/512)*128) + (ax0.ax1.fused % 512))*8) + ax2)*15) + ax3) + -15360)], placeholder[(((((((ax0.ax1.fused/512)*128) + (ax0.ax1.fused % 512))*8) + ax2)*15) + ax3)])))
      }
    }
  }
}
