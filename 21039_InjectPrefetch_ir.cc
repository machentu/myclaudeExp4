// attr [compute(T_strided_slice, 0x38a20e50)] realize_scope = ""
realize T_strided_slice([0, 1], [0, 16], [0, 64], [0, 120]) {
  produce T_strided_slice {
    parallel (ax0.ax1.fused, 0, 16) {
      for (ax2, 0, 64) {
        for (ax3, 0, 120) {
          T_strided_slice((ax0.ax1.fused/16), (ax0.ax1.fused % 16), ax2, ax3) =placeholder((ax0.ax1.fused/16), ((ax0.ax1.fused % 16) + 16), ax2, ax3)
        }
      }
    }
  }
}
