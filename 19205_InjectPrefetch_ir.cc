// attr [compute(T_cast, 0x367528d0)] realize_scope = ""
realize T_cast([0, 1], [0, 116], [0, 2520]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 116) {
      for (ax2, 0, 2520) {
        T_cast((ax0.ax1.fused/116), (ax0.ax1.fused % 116), ax2) =float32(placeholder((ax0.ax1.fused/116), (ax0.ax1.fused % 116), ax2))
      }
    }
  }
}
