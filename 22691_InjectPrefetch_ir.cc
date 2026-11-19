// attr [compute(T_add, 0x36e5a2b0)] realize_scope = ""
realize T_add([0, 1], [0, 32], [0, 16], [0, 30]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_add((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3) =int64((placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3) + placeholder((ax0.ax1.fused % 32))))
        }
      }
    }
  }
}
