// attr [compute(T_cast, 0x1ce2ec90)] realize_scope = ""
realize T_cast([0, 1], [0, 256], [0, 32], [0, 16]) {
  produce T_cast {
    for (ax1, 0, 256) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          T_cast(0, ax1, ax2, ax3) =float32(placeholder(0, ax1, ax2, ax3))
        }
      }
    }
  }
  // attr [compute(T_divide, 0x1ce2dfc0)] realize_scope = ""
  realize T_divide([0, 1], [0, 256], [0, 32], [0, 16]) {
    produce T_divide {
      for (ax1, 0, 256) {
        for (ax2, 0, 32) {
          for (ax3, 0, 16) {
            T_divide(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3)/512.000000f)
          }
        }
      }
    }
    // attr [compute(T_sum, 0x152ab5f0)] realize_scope = ""
    realize T_sum([0, 1], [0, 256], [0, 1], [0, 1]) {
      produce T_sum {
        for (ax1, 0, 256) {
          T_sum(0, ax1, 0, 0) =0.000000f
          for (k2, 0, 32) {
            for (k3, 0, 16) {
              T_sum(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0) + T_divide(0, ax1, k2, k3))
            }
          }
        }
      }
      // attr [compute(T_subtract, 0x152ab7a0)] realize_scope = ""
      realize T_subtract([0, 1], [0, 256], [0, 32], [0, 16]) {
        produce T_subtract {
          for (ax1, 0, 256) {
            for (ax2, 0, 32) {
              for (ax3, 0, 16) {
                T_subtract(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3) - T_sum(0, ax1, 0, 0))
              }
            }
          }
        }
        // attr [compute(T_power, 0x152aca00)] realize_scope = ""
        realize T_power([0, 1], [0, 256], [0, 32], [0, 16]) {
          produce T_power {
            for (ax1, 0, 256) {
              for (ax2, 0, 32) {
                for (ax3, 0, 16) {
                  T_power(0, ax1, ax2, ax3) =pow(T_subtract(0, ax1, ax2, ax3), 2.000000f)
                }
              }
            }
          }
          // attr [compute(T_sum, 0x30c92070)] realize_scope = ""
          realize T_sum([0, 1], [0, 256], [0, 1], [0, 1]) {
            produce T_sum {
              for (ax1, 0, 256) {
                T_sum(0, ax1, 0, 0) =0.000000f
                for (k2, 0, 32) {
                  for (k3, 0, 16) {
                    T_sum(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0) + T_power(0, ax1, k2, k3))
                  }
                }
              }
            }
            // attr [compute(T_divide, 0x30c926e0)] realize_scope = ""
            realize T_divide([0, 1], [0, 256], [0, 1], [0, 1]) {
              produce T_divide {
                for (ax1, 0, 256) {
                  T_divide(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0)/512.000000f)
                }
              }
              // attr [compute(T_add, 0x30c93620)] realize_scope = ""
              realize T_add([0, 1], [0, 256], [0, 1], [0, 1]) {
                produce T_add {
                  for (ax1, 0, 256) {
                    T_add(0, ax1, 0, 0) =(T_divide(0, ax1, 0, 0) + 0.000010f)
                  }
                }
                // attr [compute(T_sqrt, 0x30c93c80)] realize_scope = ""
                realize T_sqrt([0, 1], [0, 256], [0, 1], [0, 1]) {
                  produce T_sqrt {
                    for (ax1, 0, 256) {
                      T_sqrt(0, ax1, 0, 0) =sqrt(T_add(0, ax1, 0, 0))
                    }
                  }
                  // attr [compute(T_divide, 0x283ad8e0)] realize_scope = ""
                  realize T_divide([0, 1], [0, 256], [0, 32], [0, 16]) {
                    produce T_divide {
                      for (ax1, 0, 256) {
                        for (ax2, 0, 32) {
                          for (ax3, 0, 16) {
                            T_divide(0, ax1, ax2, ax3) =(T_subtract(0, ax1, ax2, ax3)/T_sqrt(0, ax1, 0, 0))
                          }
                        }
                      }
                    }
                    // attr [compute(T_cast, 0x283adc60)] realize_scope = ""
                    realize T_cast([0, 1], [0, 256], [0, 32], [0, 16]) {
                      produce T_cast {
                        for (ax1, 0, 256) {
                          for (ax2, 0, 32) {
                            for (ax3, 0, 16) {
                              T_cast(0, ax1, ax2, ax3) =float16(T_divide(0, ax1, ax2, ax3))
                            }
                          }
                        }
                      }
                      // attr [compute(T_expand_dims, 0x283ae0e0)] realize_scope = ""
                      realize T_expand_dims([0, 256], [0, 1], [0, 1]) {
                        produce T_expand_dims {
                          for (ax0, 0, 256) {
                            T_expand_dims(ax0, 0, 0) =placeholder(ax0)
                          }
                        }
                        // attr [compute(T_multiply, 0x283aece0)] realize_scope = ""
                        realize T_multiply([0, 1], [0, 256], [0, 32], [0, 16]) {
                          produce T_multiply {
                            for (ax1, 0, 256) {
                              for (ax2, 0, 32) {
                                for (ax3, 0, 16) {
                                  T_multiply(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3)*T_expand_dims(ax1, 0, 0))
                                }
                              }
                            }
                          }
                          // attr [compute(T_expand_dims, 0x283aef70)] realize_scope = ""
                          realize T_expand_dims([0, 256], [0, 1], [0, 1]) {
                            produce T_expand_dims {
                              for (ax0, 0, 256) {
                                T_expand_dims(ax0, 0, 0) =placeholder(ax0)
                              }
                            }
                            // attr [compute(T_add, 0x283afa30)] realize_scope = ""
                            realize T_add([0, 1], [0, 256], [0, 32], [0, 16]) {
                              produce T_add {
                                for (ax1, 0, 256) {
                                  for (ax2, 0, 32) {
                                    for (ax3, 0, 16) {
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
