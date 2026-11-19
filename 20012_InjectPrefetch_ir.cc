// attr [compute(T_cast, 0x3c6af3d0)] realize_scope = ""
realize T_cast([0, 4], [0, 16], [0, 1], [0, 2520]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax3, 0, 2520) {
        T_cast((ax0.ax1.fused/16), (ax0.ax1.fused % 16), 0, ax3) =float16(placeholder((ax0.ax1.fused/16), (ax0.ax1.fused % 16), 0, ax3))
      }
    }
  }
}
