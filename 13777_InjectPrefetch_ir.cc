// attr [compute(T_cast, 0x2e5447b0)] realize_scope = ""
realize T_cast([0, 1], [0, 512], [0, 8], [0, 4]) {
  produce T_cast {
    for (ax1, 0, 512) {
      for (ax2, 0, 8) {
        for (ax3, 0, 4) {
          T_cast(0, ax1, ax2, ax3) =float32(placeholder(0, ax1, ax2, ax3))
        }
      }
    }
  }
  // attr [compute(T_divide, 0x2e2ebbf0)] realize_scope = ""
  realize T_divide([0, 1], [0, 512], [0, 8], [0, 4]) {
    produce T_divide {
      for (ax1, 0, 512) {
        for (ax2, 0, 8) {
          for (ax3, 0, 4) {
            T_divide(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3)/32.000000f)
          }
        }
      }
    }
    // attr [compute(T_sum, 0x2e243fe0)] realize_scope = ""
    realize T_sum([0, 1], [0, 512], [0, 1], [0, 1]) {
      produce T_sum {
        for (ax1, 0, 512) {
          T_sum(0, ax1, 0, 0) =0.000000f
          for (k2, 0, 8) {
            for (k3, 0, 4) {
              T_sum(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0) + T_divide(0, ax1, k2, k3))
            }
          }
        }
      }
      // attr [compute(T_subtract, 0x2d5e0620)] realize_scope = ""
      realize T_subtract([0, 1], [0, 512], [0, 8], [0, 4]) {
        produce T_subtract {
          for (ax1, 0, 512) {
            for (ax2, 0, 8) {
              for (ax3, 0, 4) {
                T_subtract(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3) - T_sum(0, ax1, 0, 0))
              }
            }
          }
        }
        // attr [compute(T_power, 0x291bc2c0)] realize_scope = ""
        realize T_power([0, 1], [0, 512], [0, 8], [0, 4]) {
          produce T_power {
            for (ax1, 0, 512) {
              for (ax2, 0, 8) {
                for (ax3, 0, 4) {
                  T_power(0, ax1, ax2, ax3) =pow(T_subtract(0, ax1, ax2, ax3), 2.000000f)
                }
              }
            }
          }
          // attr [compute(T_sum, 0x2dfa7c10)] realize_scope = ""
          realize T_sum([0, 1], [0, 512], [0, 1], [0, 1]) {
            produce T_sum {
              for (ax1, 0, 512) {
                T_sum(0, ax1, 0, 0) =0.000000f
                for (k2, 0, 8) {
                  for (k3, 0, 4) {
                    T_sum(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0) + T_power(0, ax1, k2, k3))
                  }
                }
              }
            }
            // attr [compute(T_divide, 0x31450e60)] realize_scope = ""
            realize T_divide([0, 1], [0, 512], [0, 1], [0, 1]) {
              produce T_divide {
                for (ax1, 0, 512) {
                  T_divide(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0)/32.000000f)
                }
              }
              // attr [compute(T_add, 0x2d9b27c0)] realize_scope = ""
              realize T_add([0, 1], [0, 512], [0, 1], [0, 1]) {
                produce T_add {
                  for (ax1, 0, 512) {
                    T_add(0, ax1, 0, 0) =(T_divide(0, ax1, 0, 0) + 0.000010f)
                  }
                }
                // attr [compute(T_sqrt, 0x2d5e8290)] realize_scope = ""
                realize T_sqrt([0, 1], [0, 512], [0, 1], [0, 1]) {
                  produce T_sqrt {
                    for (ax1, 0, 512) {
                      T_sqrt(0, ax1, 0, 0) =sqrt(T_add(0, ax1, 0, 0))
                    }
                  }
                  // attr [compute(T_divide, 0x2a6b9080)] realize_scope = ""
                  realize T_divide([0, 1], [0, 512], [0, 8], [0, 4]) {
                    produce T_divide {
                      for (ax1, 0, 512) {
                        for (ax2, 0, 8) {
                          for (ax3, 0, 4) {
                            T_divide(0, ax1, ax2, ax3) =(T_subtract(0, ax1, ax2, ax3)/T_sqrt(0, ax1, 0, 0))
                          }
                        }
                      }
                    }
                    // attr [compute(T_cast, 0x2e7c77a0)] realize_scope = ""
                    realize T_cast([0, 1], [0, 512], [0, 8], [0, 4]) {
                      produce T_cast {
                        for (ax1, 0, 512) {
                          for (ax2, 0, 8) {
                            for (ax3, 0, 4) {
                              T_cast(0, ax1, ax2, ax3) =float16(T_divide(0, ax1, ax2, ax3))
                            }
                          }
                        }
                      }
                      // attr [compute(T_expand_dims, 0x153c3400)] realize_scope = ""
                      realize T_expand_dims([0, 512], [0, 1], [0, 1]) {
                        produce T_expand_dims {
                          for (ax0, 0, 512) {
                            T_expand_dims(ax0, 0, 0) =placeholder(ax0)
                          }
                        }
                        // attr [compute(T_multiply, 0x1ce5a290)] realize_scope = ""
                        realize T_multiply([0, 1], [0, 512], [0, 8], [0, 4]) {
                          produce T_multiply {
                            for (ax1, 0, 512) {
                              for (ax2, 0, 8) {
                                for (ax3, 0, 4) {
                                  T_multiply(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3)*T_expand_dims(ax1, 0, 0))
                                }
                              }
                            }
                          }
                          // attr [compute(T_expand_dims, 0x26601f20)] realize_scope = ""
                          realize T_expand_dims([0, 512], [0, 1], [0, 1]) {
                            produce T_expand_dims {
                              for (ax0, 0, 512) {
                                T_expand_dims(ax0, 0, 0) =placeholder(ax0)
                              }
                            }
                            // attr [compute(T_add, 0x2d7f1690)] realize_scope = ""
                            realize T_add([0, 1], [0, 512], [0, 8], [0, 4]) {
                              produce T_add {
                                for (ax1, 0, 512) {
                                  for (ax2, 0, 8) {
                                    for (ax3, 0, 4) {
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
