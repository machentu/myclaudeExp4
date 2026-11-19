// attr [compute(T_add, 0x1c0167f0)] realize_scope = ""
realize T_add([0, 1], [0, 8], [0, 1], [0, 1]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 8) {
      T_add((ax0.ax1.fused/8), (ax0.ax1.fused % 8), 0, 0) =int64((placeholder((ax0.ax1.fused/8), (ax0.ax1.fused % 8), 0, 0) + placeholder((ax0.ax1.fused % 8))))
    }
  }
}
