produce T_concat {
  parallel (ax0.ax1.fused, 0, 56) {
    for (ax2, 0, 2520) {
      T_concat[((ax0.ax1.fused*2520) + ax2)] = tvm_if_then_else((5 <= ax0.ax1.fused), placeholder[(((ax0.ax1.fused*2520) + ax2) + -12600)], tvm_if_then_else((4 <= ax0.ax1.fused), placeholder[(((ax0.ax1.fused*2520) + ax2) + -10080)], placeholder[((ax0.ax1.fused*2520) + ax2)]))
    }
  }
}
