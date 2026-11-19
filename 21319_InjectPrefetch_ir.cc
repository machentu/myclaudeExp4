// attr [compute(T_strided_slice, 0x2d97e4c0)] realize_scope = ""
realize T_strided_slice([0, 1], [0, 64], [0, 16], [0, 30]) {
  produce T_strided_slice {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_strided_slice((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3) =placeholder((ax0.ax1.fused/64), ((ax0.ax1.fused % 64) + 64), ax2, ax3)
        }
      }
    }
  }
}
