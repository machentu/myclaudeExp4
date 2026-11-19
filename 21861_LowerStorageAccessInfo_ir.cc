produce T_concat {
  parallel (ax0.ax1.fused, 0, 192) {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_concat[((((ax0.ax1.fused*16) + ax2)*30) + ax3)] = tvm_if_then_else((64 <= ax0.ax1.fused), placeholder[(((((ax0.ax1.fused*16) + ax2)*30) + ax3) + -30720)], placeholder[((((ax0.ax1.fused*16) + ax2)*30) + ax3)])
      }
    }
  }
}
