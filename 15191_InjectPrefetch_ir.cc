// attr [compute(T_cast, 0x2663b090)] realize_scope = ""
realize T_cast([0, 1], [0, 64], [0, 1], [0, 1]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 64) {
      T_cast((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0) =float16(placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0))
    }
  }
}
