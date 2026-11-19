// attr [compute(T_reshape, 0x26e53b30)] realize_scope = ""
realize T_reshape([0, 1], [0, 512]) {
  produce T_reshape {
    for (ax1, 0, 512) {
      T_reshape(0, ax1) =placeholder(0, 0, (((0*512) + ax1) % 512))
    }
  }
}
