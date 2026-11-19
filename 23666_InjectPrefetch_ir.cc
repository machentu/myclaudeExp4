// attr [compute(T_strided_slice, 0x4370a7e0)] realize_scope = ""
realize T_strided_slice([0, 1], [0, 17], [0, 1], [0, 2520]) {
  produce T_strided_slice {
    parallel (ax0.ax1.fused, 0, 17) {
      for (ax3, 0, 2520) {
        T_strided_slice((ax0.ax1.fused/17), (ax0.ax1.fused % 17), 0, ax3) =placeholder((ax0.ax1.fused/17), (ax0.ax1.fused % 17), (0 + 2), ax3)
      }
    }
  }
}
