// attr [compute(T_cast, 0x21d27630)] realize_scope = ""
realize T_cast([0, 1], [0, 512]) {
  produce T_cast {
    for (ax1, 0, 512) {
      T_cast(0, ax1) =float32(placeholder(0, ax1))
    }
  }
}
