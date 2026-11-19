// attr [compute(T_add, 0x310b21f0)] realize_scope = ""
realize T_add([0, 1], [0, 96], [0, 16], [0, 8]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 96) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_add((ax0.ax1.fused/96), (ax0.ax1.fused % 96), ax2, ax3) =(placeholder((ax0.ax1.fused/96), (ax0.ax1.fused % 96), ax2, ax3) + placeholder((ax0.ax1.fused/96), (ax0.ax1.fused % 96), ax2, ax3))
        }
      }
    }
  }
}
