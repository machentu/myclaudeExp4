// attr [compute(T_transpose, 0x2d7a78e0)] realize_scope = ""
realize T_transpose([0, 1], [0, 1], [0, 512]) {
  produce T_transpose {
    for (ax2, 0, 512) {
      T_transpose(0, 0, ax2) =placeholder(0, ax2, 0)
    }
  }
}
