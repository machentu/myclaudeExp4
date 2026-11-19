// attr [compute(T_add, 0x38f928b0)] realize_scope = ""
realize T_add([0, 1], [0, 80], [0, 8], [0, 15]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 80) {
      for (ax2, 0, 8) {
        for (ax3, 0, 15) {
          T_add((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2, ax3) =int64((placeholder((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2, ax3) + placeholder((ax0.ax1.fused % 80))))
        }
      }
    }
  }
}
