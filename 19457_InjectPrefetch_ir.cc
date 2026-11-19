// attr [compute(T_strided_slice, 0x3842fb50)] realize_scope = ""
realize T_strided_slice([0, 1], [0, 64], [0, 2520]) {
  produce T_strided_slice {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax2, 0, 2520) {
        T_strided_slice((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2) =placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2)
      }
    }
  }
}
