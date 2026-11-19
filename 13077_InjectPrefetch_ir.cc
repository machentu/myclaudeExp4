// attr [compute(T_relu, 0x145e00f0)] realize_scope = ""
realize T_relu([0, 1], [0, 512]) {
  produce T_relu {
    for (ax1, 0, 512) {
      T_relu(0, ax1) =max(placeholder(0, ax1), 0.000000h)
    }
  }
}
