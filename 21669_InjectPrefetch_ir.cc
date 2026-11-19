// attr [compute(T_add, 0x3780e9c0)] realize_scope = ""
realize T_add([0, 1], [0, 80], [0, 32], [0, 60]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 80) {
      for (ax2, 0, 32) {
        for (ax3, 0, 60) {
          T_add((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2, ax3) =int64((placeholder((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2, ax3) + placeholder((ax0.ax1.fused % 80))))
        }
      }
    }
  }
}
