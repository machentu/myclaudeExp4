// attr [compute(T_add, 0x2d8028d0)] realize_scope = ""
realize T_add([0, 1], [0, 512], [0, 8], [0, 4]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 512) {
      for (ax2, 0, 8) {
        for (ax3, 0, 4) {
          T_add((ax0.ax1.fused/512), (ax0.ax1.fused % 512), ax2, ax3) =(int32(placeholder((ax0.ax1.fused/512), (ax0.ax1.fused % 512), ax2, ax3)) + int32(placeholder((ax0.ax1.fused/512), (ax0.ax1.fused % 512), ax2, ax3)))
        }
      }
    }
  }
}
