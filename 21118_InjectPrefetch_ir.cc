// attr [compute(T_strided_slice, 0x46f26e60)] realize_scope = ""
realize T_strided_slice([0, 1], [0, 128], [0, 8], [0, 15]) {
  produce T_strided_slice {
    parallel (ax0.ax1.fused, 0, 128) {
      for (ax2, 0, 8) {
        for (ax3, 0, 15) {
          T_strided_slice((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) =placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3)
        }
      }
    }
  }
}
