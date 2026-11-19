// attr [compute(T_add, 0x1b039800)] realize_scope = ""
realize T_add([0, 1], [0, 4], [0, 1], [0, 1]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 4) {
      T_add((ax0.ax1.fused/4), (ax0.ax1.fused % 4), 0, 0) =int64((placeholder((ax0.ax1.fused/4), (ax0.ax1.fused % 4), 0, 0) + placeholder((ax0.ax1.fused % 4))))
    }
  }
}
