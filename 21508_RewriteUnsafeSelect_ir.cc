produce T_concat {
  parallel (ax0.ax1.fused, 0, 48) {
    for (ax2, 0, 64) {
      for (ax3, 0, 120) {
        T_concat[((((ax0.ax1.fused*64) + ax2)*120) + ax3)] = tvm_if_then_else((32 <= ax0.ax1.fused), placeholder[(((((ax0.ax1.fused*64) + ax2)*120) + ax3) + -245760)], tvm_if_then_else((16 <= ax0.ax1.fused), placeholder[(((((ax0.ax1.fused*64) + ax2)*120) + ax3) + -122880)], placeholder[((((ax0.ax1.fused*64) + ax2)*120) + ax3)]))
      }
    }
  }
}
