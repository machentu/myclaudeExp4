// attr [compute(T_cast, 0x43b1d060)] realize_scope = ""
realize T_cast([0, 1], [0, 1], [0, 2520]) {
  produce T_cast {
    for (ax2, 0, 2520) {
      T_cast(0, 0, ax2) =float16(placeholder(0, 0, ax2))
    }
  }
}
