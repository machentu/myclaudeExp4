// attr [compute(T_multiply, 0x311b7690)] realize_scope = ""
realize T_multiply([0, 1], [0, 256], [0, 32], [0, 16]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 256) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          T_multiply((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) =(placeholder((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3)*placeholder())
        }
      }
    }
  }
}
