// attr [compute(T_cast, 0x3ee50f30)] realize_scope = ""
realize T_cast([0, 1], [0, 4], [0, 2520]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 4) {
      for (ax2, 0, 2520) {
        T_cast((ax0.ax1.fused/4), (ax0.ax1.fused % 4), ax2) =float16(placeholder((ax0.ax1.fused/4), (ax0.ax1.fused % 4), ax2))
      }
    }
  }
}
