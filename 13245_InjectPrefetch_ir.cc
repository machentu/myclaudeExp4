// attr [compute(T_reshape, 0x1c9c77d0)] realize_scope = ""
realize T_reshape([0, 1], [0, 1], [0, 512]) {
  produce T_reshape {
    for (ax2, 0, 512) {
      T_reshape(0, 0, ax2) =placeholder(0, ((((0 + 0)*512) + ax2) % 512), 0, 0)
    }
  }
}
