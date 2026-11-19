// attr [compute(T_cast, 0x481083c0)] realize_scope = ""
realize T_cast([0, 1], [0, 56], [0, 2520]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 56) {
      for (ax2, 0, 2520) {
        T_cast((ax0.ax1.fused/56), (ax0.ax1.fused % 56), ax2) =float32(placeholder((ax0.ax1.fused/56), (ax0.ax1.fused % 56), ax2))
      }
    }
  }
}
