// attr [compute(T_cast, 0x2dc84090)] realize_scope = ""
realize T_cast([0, 1], [0, 128], [0, 1], [0, 1]) {
  produce T_cast {
    parallel (ax0.ax1.fused, 0, 128) {
      T_cast((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0) =float16(placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0))
    }
  }
}
