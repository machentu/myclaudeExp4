// attr [compute(T_multiply, 0x2ea3ca90)] realize_scope = ""
realize T_multiply([0, 1], [0, 384], [0, 16], [0, 8]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 384) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_multiply((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =(placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3)*placeholder())
        }
      }
    }
  }
}
