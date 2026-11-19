// attr [compute(T_add, 0x490dce80)] realize_scope = ""
realize T_add([0, 1], [0, 128], [0, 8], [0, 15]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 128) {
      for (ax2, 0, 8) {
        for (ax3, 0, 15) {
          T_add((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) =int64((placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) + placeholder((ax0.ax1.fused % 128))))
        }
      }
    }
  }
}
