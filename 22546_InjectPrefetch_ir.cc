// attr [compute(T_add, 0x4dc7f5e0)] realize_scope = ""
realize T_add([0, 1], [0, 1], [0, 16], [0, 30]) {
  produce T_add {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_add(0, 0, ax2, ax3) =int64((placeholder(0, 0, ax2, ax3) + placeholder(0)))
      }
    }
  }
}
