produce T_concat {
  parallel (ax0.ax1.fused, 0, 56) {
    for (ax2, 0, 2520) {
      T_concat[((ax0.ax1.fused*2520) + ax2)] = tvm_if_then_else(((((ax0.ax1.fused % 56) - 4) - 1) >= 0), placeholder[((((((ax0.ax1.fused/56)*51) + (ax0.ax1.fused % 56))*2520) + ax2) + -12600)], tvm_if_then_else((((ax0.ax1.fused % 56) - 4) >= 0), placeholder[(((((ax0.ax1.fused/56) + (ax0.ax1.fused % 56))*2520) + ax2) + -10080)], placeholder[(((((ax0.ax1.fused/56)*4) + (ax0.ax1.fused % 56))*2520) + ax2)]))
    }
  }
}
