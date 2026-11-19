// attr [compute(T_add, 0x2eb6bed0)] realize_scope = ""
realize T_add([0, 1], [0, 256], [0, 32], [0, 16]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 256) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          T_add((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) =(int32(placeholder((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3)) + int32(placeholder((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3)))
        }
      }
    }
  }
}
