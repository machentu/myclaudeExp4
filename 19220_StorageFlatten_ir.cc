produce T_concat {
  parallel (ax0.ax1.fused, 0, 116) {
    for (ax2, 0, 2520) {
      T_concat[((ax0.ax1.fused*2520) + ax2)] = tvm_if_then_else(((((ax0.ax1.fused % 116) - 4) - 80) >= 0), placeholder[((((((ax0.ax1.fused/116)*32) + (ax0.ax1.fused % 116))*2520) + ax2) + -211680)], tvm_if_then_else((((ax0.ax1.fused % 116) - 4) >= 0), placeholder[((((((ax0.ax1.fused/116)*80) + (ax0.ax1.fused % 116))*2520) + ax2) + -10080)], placeholder[(((((ax0.ax1.fused/116)*4) + (ax0.ax1.fused % 116))*2520) + ax2)]))
    }
  }
}
