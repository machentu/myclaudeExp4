// attr [compute(T_strided_slice, 0x48aa1600)] realize_scope = ""
realize T_strided_slice([0, 1], [0, 1], [0, 2520]) {
  produce T_strided_slice {
    for (ax2, 0, 2520) {
      T_strided_slice(0, 0, ax2) =placeholder(0, (0 + 64), ax2)
    }
  }
}
