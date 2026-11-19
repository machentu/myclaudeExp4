// attr [compute(T_add, 0x2e30da30)] realize_scope = ""
realize T_add([0, 1], [0, 384], [0, 16], [0, 8]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 384) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_add((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =(int32(placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3)) + int32(placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3)))
        }
      }
    }
  }
}
