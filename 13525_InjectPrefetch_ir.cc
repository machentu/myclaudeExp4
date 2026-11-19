// attr [compute(T_multiply, 0x25be4e50)] realize_scope = ""
realize T_multiply([0, 1], [0, 128], [0, 8], [0, 4]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 128) {
      for (ax2, 0, 8) {
        for (ax3, 0, 4) {
          T_multiply((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) =(placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3)*placeholder())
        }
      }
    }
  }
}
