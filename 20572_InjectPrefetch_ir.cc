// attr [compute(T_add, 0x46a3db70)] realize_scope = ""
realize T_add([0, 1], [0, 128], [0, 16], [0, 30]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 128) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_add((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) =int64((placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) + placeholder((ax0.ax1.fused % 128))))
        }
      }
    }
  }
}
