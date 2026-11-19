// attr [compute(T_cast, 0x2e7945f0)] realize_scope = ""
realize T_cast([0, 1], [0, 64], [0, 64], [0, 32]) {
  produce T_cast {
    for (ax1, 0, 64) {
      for (ax2, 0, 64) {
        for (ax3, 0, 32) {
          T_cast(0, ax1, ax2, ax3) =float32(placeholder(0, ax1, ax2, ax3))
        }
      }
    }
  }
  // attr [compute(T_divide, 0x2da991e0)] realize_scope = ""
  realize T_divide([0, 1], [0, 64], [0, 64], [0, 32]) {
    produce T_divide {
      for (ax1, 0, 64) {
        for (ax2, 0, 64) {
          for (ax3, 0, 32) {
            T_divide(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3)/2048.000000f)
          }
        }
      }
    }
    // attr [compute(T_sum, 0x2da99390)] realize_scope = ""
    realize T_sum([0, 1], [0, 64], [0, 1], [0, 1]) {
      produce T_sum {
        for (ax1, 0, 64) {
          T_sum(0, ax1, 0, 0) =0.000000f
          for (k2, 0, 64) {
            for (k3, 0, 32) {
              T_sum(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0) + T_divide(0, ax1, k2, k3))
            }
          }
        }
      }
      // attr [compute(T_subtract, 0x3140ebe0)] realize_scope = ""
      realize T_subtract([0, 1], [0, 64], [0, 64], [0, 32]) {
        produce T_subtract {
          for (ax1, 0, 64) {
            for (ax2, 0, 64) {
              for (ax3, 0, 32) {
                T_subtract(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3) - T_sum(0, ax1, 0, 0))
              }
            }
          }
        }
        // attr [compute(T_power, 0x3140efa0)] realize_scope = ""
        realize T_power([0, 1], [0, 64], [0, 64], [0, 32]) {
          produce T_power {
            for (ax1, 0, 64) {
              for (ax2, 0, 64) {
                for (ax3, 0, 32) {
                  T_power(0, ax1, ax2, ax3) =pow(T_subtract(0, ax1, ax2, ax3), 2.000000f)
                }
              }
            }
          }
          // attr [compute(T_sum, 0x2e4cc890)] realize_scope = ""
          realize T_sum([0, 1], [0, 64], [0, 1], [0, 1]) {
            produce T_sum {
              for (ax1, 0, 64) {
                T_sum(0, ax1, 0, 0) =0.000000f
                for (k2, 0, 64) {
                  for (k3, 0, 32) {
                    T_sum(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0) + T_power(0, ax1, k2, k3))
                  }
                }
              }
            }
            // attr [compute(T_divide, 0x2e4ccf00)] realize_scope = ""
            realize T_divide([0, 1], [0, 64], [0, 1], [0, 1]) {
              produce T_divide {
                for (ax1, 0, 64) {
                  T_divide(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0)/2048.000000f)
                }
              }
              // attr [compute(T_add, 0x2e4cde40)] realize_scope = ""
              realize T_add([0, 1], [0, 64], [0, 1], [0, 1]) {
                produce T_add {
                  for (ax1, 0, 64) {
                    T_add(0, ax1, 0, 0) =(T_divide(0, ax1, 0, 0) + 0.000010f)
                  }
                }
                // attr [compute(T_sqrt, 0x2e4ce4a0)] realize_scope = ""
                realize T_sqrt([0, 1], [0, 64], [0, 1], [0, 1]) {
                  produce T_sqrt {
                    for (ax1, 0, 64) {
                      T_sqrt(0, ax1, 0, 0) =sqrt(T_add(0, ax1, 0, 0))
                    }
                  }
                  // attr [compute(T_divide, 0x2e4cf180)] realize_scope = ""
                  realize T_divide([0, 1], [0, 64], [0, 64], [0, 32]) {
                    produce T_divide {
                      for (ax1, 0, 64) {
                        for (ax2, 0, 64) {
                          for (ax3, 0, 32) {
                            T_divide(0, ax1, ax2, ax3) =(T_subtract(0, ax1, ax2, ax3)/T_sqrt(0, ax1, 0, 0))
                          }
                        }
                      }
                    }
                    // attr [compute(T_cast, 0x2e4cf500)] realize_scope = ""
                    realize T_cast([0, 1], [0, 64], [0, 64], [0, 32]) {
                      produce T_cast {
                        for (ax1, 0, 64) {
                          for (ax2, 0, 64) {
                            for (ax3, 0, 32) {
                              T_cast(0, ax1, ax2, ax3) =float16(T_divide(0, ax1, ax2, ax3))
                            }
                          }
                        }
                      }
                      // attr [compute(T_expand_dims, 0x2e4cf980)] realize_scope = ""
                      realize T_expand_dims([0, 64], [0, 1], [0, 1]) {
                        produce T_expand_dims {
                          for (ax0, 0, 64) {
                            T_expand_dims(ax0, 0, 0) =placeholder(ax0)
                          }
                        }
                        // attr [compute(T_multiply, 0x2e4d0580)] realize_scope = ""
                        realize T_multiply([0, 1], [0, 64], [0, 64], [0, 32]) {
                          produce T_multiply {
                            for (ax1, 0, 64) {
                              for (ax2, 0, 64) {
                                for (ax3, 0, 32) {
                                  T_multiply(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3)*T_expand_dims(ax1, 0, 0))
                                }
                              }
                            }
                          }
                          // attr [compute(T_expand_dims, 0x291cba70)] realize_scope = ""
                          realize T_expand_dims([0, 64], [0, 1], [0, 1]) {
                            produce T_expand_dims {
                              for (ax0, 0, 64) {
                                T_expand_dims(ax0, 0, 0) =placeholder(ax0)
                              }
                            }
                            // attr [compute(T_add, 0x291cc530)] realize_scope = ""
                            realize T_add([0, 1], [0, 64], [0, 64], [0, 32]) {
                              produce T_add {
                                for (ax1, 0, 64) {
                                  for (ax2, 0, 64) {
                                    for (ax3, 0, 32) {
                                      T_add(0, ax1, ax2, ax3) =(T_multiply(0, ax1, ax2, ax3) + T_expand_dims(ax1, 0, 0))
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
