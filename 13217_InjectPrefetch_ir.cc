// attr [compute(T_add, 0x31b26780)] realize_scope = ""
realize T_add([0, 1], [0, 512], [0, 1]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 512) {
      T_add((ax0.ax1.fused/512), (ax0.ax1.fused % 512), 0) =int64((placeholder((ax0.ax1.fused/512), (ax0.ax1.fused % 512), 0) + placeholder((ax0.ax1.fused % 512))))
    }
  }
}
