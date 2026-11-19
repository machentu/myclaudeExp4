produce T_concat {
  parallel (ax0.ax1.fused, 0, 51) {
    for (ax2, 0, 2520) {
      T_concat[((ax0.ax1.fused*2520) + ax2)] = tvm_if_then_else((2400 <= ax2), placeholder[(((ax0.ax1.fused*120) + ax2) + -2400)], tvm_if_then_else((1920 <= ax2), placeholder[(((ax0.ax1.fused*480) + ax2) + -1920)], placeholder[((ax0.ax1.fused*1920) + ax2)]))
    }
  }
}
