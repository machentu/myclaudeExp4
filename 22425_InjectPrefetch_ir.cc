// attr [compute(T_cast, 0x38c44e20)] realize_scope = ""
realize T_cast([0, 1], [0, 80], [0, 2520]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 80) {
      for (ax2, 0, 2520) {
        T_cast((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2) =float16(placeholder((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2))
      }
    }
  }
}
