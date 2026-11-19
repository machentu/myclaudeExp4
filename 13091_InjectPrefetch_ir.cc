// attr [compute(T_add, 0x2d94fe60)] realize_scope = ""
realize T_add([0, 1], [0, 512]) {
  produce T_add {
    for (ax1, 0, 512) {
      T_add(0, ax1) =(placeholder(0, ax1) + placeholder(ax1))
    }
  }
}
