// attr [compute(T_add, 0x3f449d60)] realize_scope = ""
realize T_add([0, 1], [0, 17], [0, 2], [0, 2520]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 17) {
      for (ax2, 0, 2) {
        for (ax3, 0, 2520) {
          T_add((ax0.ax1.fused/17), (ax0.ax1.fused % 17), ax2, ax3) =(placeholder((ax0.ax1.fused/17), (ax0.ax1.fused % 17), ax2, ax3) + placeholder(ax2, ax3))
        }
      }
    }
  }
}
