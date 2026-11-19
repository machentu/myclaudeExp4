// attr [compute(T_add, 0x2199f8b0)] realize_scope = ""
realize T_add([0, 1], [0, 64], [0, 1], [0, 1]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 64) {
      T_add((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0) =int64((placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0) + placeholder((ax0.ax1.fused % 64))))
    }
  }
}
