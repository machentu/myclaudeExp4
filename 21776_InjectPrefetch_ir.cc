// attr [compute(T_add, 0x48e17ae0)] realize_scope = ""
realize T_add([0, 1], [0, 16], [0, 64], [0, 120]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 16) {
      for (ax2, 0, 64) {
        for (ax3, 0, 120) {
          T_add((ax0.ax1.fused/16), (ax0.ax1.fused % 16), ax2, ax3) =int64((placeholder((ax0.ax1.fused/16), (ax0.ax1.fused % 16), ax2, ax3) + placeholder((ax0.ax1.fused % 16))))
        }
      }
    }
  }
}
