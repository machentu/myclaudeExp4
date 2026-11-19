produce T_concat {
  parallel (ax0.ax1.fused, 0, 65) {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_concat[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] = tvm_if_then_else((64 <= ax0.ax1.fused), placeholder[(((((ax0.ax1.fused*8) + ax2)*15) + ax3) + -7680)], placeholder[((((ax0.ax1.fused*8) + ax2)*15) + ax3)])
      }
    }
  }
}
