// attr [compute(T_multiply, 0x4949cb60)] realize_scope = ""
realize T_multiply([0, 1], [0, 1], [0, 8], [0, 15]) {
  produce T_multiply {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_multiply(0, 0, ax2, ax3) =(int64(placeholder(0, 0, ax2, ax3))*int64(placeholder()))
      }
    }
  }
}
