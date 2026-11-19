// attr [compute(T_add, 0x1bfd27a0)] realize_scope = ""
realize T_add([0, 1], [0, 128], [0, 1], [0, 1]) {
  produce T_add {
    parallel (ax0.ax1.fused, 0, 128) {
      T_add((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0) =int64((placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0) + placeholder((ax0.ax1.fused % 128))))
    }
  }
}
