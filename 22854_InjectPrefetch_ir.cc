// attr [compute(T_sigmoid, 0x3f3e22f0)] realize_scope = ""
realize T_sigmoid([0, 1], [0, 1], [0, 2520]) {
  produce T_sigmoid {
    for (ax2, 0, 2520) {
      T_sigmoid(0, 0, ax2) =sigmoid(placeholder(0, 0, ax2))
    }
  }
}
