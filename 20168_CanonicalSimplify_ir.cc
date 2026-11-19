produce T_concat {
  parallel (ax0.ax1.fused, 0, 65) {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_concat[(((ax0.ax1.fused*1920) + (ax2*60)) + ax3)] = tvm_if_then_else((64 <= ax0.ax1.fused), placeholder[((((ax0.ax1.fused*1920) + (ax2*60)) + ax3) - 122880)], placeholder[(((ax0.ax1.fused*1920) + (ax2*60)) + ax3)])
      }
    }
  }
}
