// attr [compute(T_multiply, 0x49f07020)] realize_scope = ""
realize T_multiply([0, 1], [0, 384], [0, 16], [0, 30]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 384) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_multiply((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =(int64(placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3))*int64(placeholder((ax0.ax1.fused % 384), 0, 0)))
        }
      }
    }
  }
}
