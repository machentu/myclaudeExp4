produce T_concat {
  parallel (ax0.ax1.fused, 0, 17) {
    for (ax2, 0, 3) {
      for (ax3, 0, 2520) {
        T_concat[((((ax0.ax1.fused*3) + ax2)*2520) + ax3)] = tvm_if_then_else((2 <= ax2), placeholder[((((ax0.ax1.fused + ax2)*2520) + ax3) + -5040)], placeholder[((((ax0.ax1.fused*2) + ax2)*2520) + ax3)])
      }
    }
  }
}
