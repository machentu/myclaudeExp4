// attr [compute(T_cast, 0x2e5c60b0)] realize_scope = ""
realize T_cast([0, 1], [0, 96], [0, 1], [0, 1]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 96) {
      T_cast((ax0.ax1.fused/96), (ax0.ax1.fused % 96), 0, 0) =float16(placeholder((ax0.ax1.fused/96), (ax0.ax1.fused % 96), 0, 0))
    }
  }
}
