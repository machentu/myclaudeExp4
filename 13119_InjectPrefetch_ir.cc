// attr [compute(T_cast, 0x2e073130)] realize_scope = ""
realize T_cast([0, 1], [0, 512]) {
  produce T_cast {
    for (ax1, 0, 512) {
      T_cast(0, ax1) =float16(placeholder(0, ax1))
    }
  }
}
