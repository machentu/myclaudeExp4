// attr [compute(T_multiply, 0x47f79e10)] realize_scope = ""
realize T_multiply([0, 1], [0, 1], [0, 32], [0, 60]) {
  produce T_multiply {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_multiply(0, 0, ax2, ax3) =(int64(placeholder(0, 0, ax2, ax3))*int64(placeholder()))
      }
    }
  }
}
