// attr [compute(T_multiply, 0x48cc1e50)] realize_scope = ""
realize T_multiply([0, 1], [0, 1], [0, 2520]) {
  produce T_multiply {
    for (ax2, 0, 2520) {
      T_multiply(0, 0, ax2) =(placeholder(0, 0, ax2)*placeholder())
    }
  }
}
