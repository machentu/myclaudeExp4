// attr [compute(T_add, 0x363f1b80)] realize_scope = ""
realize T_add([0, 1], [0, 80], [0, 16], [0, 30]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 80) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_add((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2, ax3) =int64((placeholder((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2, ax3) + placeholder((ax0.ax1.fused % 80))))
        }
      }
    }
  }
}
