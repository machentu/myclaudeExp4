// attr [compute(T_add, 0x45607100)] realize_scope = ""
realize T_add([0, 1], [0, 1], [0, 32], [0, 60]) {
  produce T_add {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_add(0, 0, ax2, ax3) =int64((placeholder(0, 0, ax2, ax3) + placeholder(0)))
      }
    }
  }
}
