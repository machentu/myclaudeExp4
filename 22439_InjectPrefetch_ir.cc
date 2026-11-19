// attr [compute(T_strided_slice, 0x38e12390)] realize_scope = ""
realize T_strided_slice([0, 1], [0, 80], [0, 2520]) {
  produce T_strided_slice {
    parallel (ax0.ax1.fused, 0, 80) {
      for (ax2, 0, 2520) {
        T_strided_slice((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2) =placeholder((ax0.ax1.fused/80), ((ax0.ax1.fused % 80) + 64), ax2)
      }
    }
  }
}
