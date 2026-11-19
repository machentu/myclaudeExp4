// attr [compute(T_strided_slice, 0x2aea8be0)] realize_scope = ""
realize T_strided_slice([0, 1], [0, 32], [0, 32], [0, 60]) {
  produce T_strided_slice {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 32) {
        for (ax3, 0, 60) {
          T_strided_slice((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3) =placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2, ax3)
        }
      }
    }
  }
}
