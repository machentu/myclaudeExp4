produce T_concat {
  parallel (ax0.ax1.fused, 0, 48) {
    for (ax2, 0, 64) {
      for (ax3, 0, 120) {
        T_concat[((((ax0.ax1.fused*64) + ax2)*120) + ax3)] = tvm_if_then_else(((((ax0.ax1.fused % 48) - 16) - 16) >= 0), placeholder[((((((((ax0.ax1.fused/48)*16) + (ax0.ax1.fused % 48))*64) + ax2)*120) + ax3) + -245760)], tvm_if_then_else((((ax0.ax1.fused % 48) - 16) >= 0), placeholder[((((((((ax0.ax1.fused/48)*16) + (ax0.ax1.fused % 48))*64) + ax2)*120) + ax3) + -122880)], placeholder[(((((((ax0.ax1.fused/48)*16) + (ax0.ax1.fused % 48))*64) + ax2)*120) + ax3)]))
      }
    }
  }
}
