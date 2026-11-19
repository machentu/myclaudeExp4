// attr [compute(T_multiply, 0x2af195b0)] realize_scope = ""
realize T_multiply([0, 1], [0, 512]) {
  produce T_multiply {
    for (ax1, 0, 512) {
      T_multiply(0, ax1) =(placeholder(0, ax1)*placeholder(ax1))
    }
  }
}
