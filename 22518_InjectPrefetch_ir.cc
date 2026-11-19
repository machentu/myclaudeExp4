// attr [compute(T_multiply, 0x47e59ef0)] realize_scope = ""
realize T_multiply([0, 1], [0, 1], [0, 16], [0, 30]) {
  produce T_multiply {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_multiply(0, 0, ax2, ax3) =(int64(placeholder(0, 0, ax2, ax3))*int64(placeholder()))
      }
    }
  }
}
