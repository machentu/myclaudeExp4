assert((num_args == 4), "fused_nn_instance_norm_1: num_args should be 4")
let arg0 = tvm_struct_get(args, 0, 12)
let arg0.code = arg_type_ids[0]
let arg1 = tvm_struct_get(args, 1, 12)
let arg1.code = arg_type_ids[1]
let arg2 = tvm_struct_get(args, 2, 12)
let arg2.code = arg_type_ids[2]
let arg3 = tvm_struct_get(args, 3, 12)
let arg3.code = arg_type_ids[3]
let placeholder = tvm_struct_get(arg0, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg0.shape = tvm_struct_get(arg0, 0, 2)
let arg0.strides = tvm_struct_get(arg0, 0, 3)
if (!tvm_handle_is_null(arg0.strides)) {
  assert(((((1 == int32(arg0.strides[3])) && (8 == int32(arg0.strides[2]))) && (128 == int32(arg0.strides[1]))) && (49152 == int32(arg0.strides[0]))), "arg0.strides: expected to be compact array")
  0
}
let dev_type = tvm_struct_get(arg0, 0, 10)
let dev_id = tvm_struct_get(arg0, 0, 9)
let placeholder = tvm_struct_get(arg1, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg1.shape = tvm_struct_get(arg1, 0, 2)
let arg1.strides = tvm_struct_get(arg1, 0, 3)
if (!tvm_handle_is_null(arg1.strides)) {
  assert((1 == int32(arg1.strides[0])), "arg1.strides: expected to be compact array")
  0
}
let placeholder = tvm_struct_get(arg2, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg2.shape = tvm_struct_get(arg2, 0, 2)
let arg2.strides = tvm_struct_get(arg2, 0, 3)
if (!tvm_handle_is_null(arg2.strides)) {
  assert((1 == int32(arg2.strides[0])), "arg2.strides: expected to be compact array")
  0
}
let T_add = tvm_struct_get(arg3, 0, 1)
// attr [T_add] storage_alignment = 64
let arg3.shape = tvm_struct_get(arg3, 0, 2)
let arg3.strides = tvm_struct_get(arg3, 0, 3)
if (!tvm_handle_is_null(arg3.strides)) {
  assert(((((1 == int32(arg3.strides[3])) && (8 == int32(arg3.strides[2]))) && (128 == int32(arg3.strides[1]))) && (49152 == int32(arg3.strides[0]))), "arg3.strides: expected to be compact array")
  0
}
assert(((((arg0.code == 3) || (arg0.code == 13)) || (arg0.code == 7)) || (arg0.code == 4)), "fused_nn_instance_norm_1: Expect arg[0] to be pointer")
assert(((((arg1.code == 3) || (arg1.code == 13)) || (arg1.code == 7)) || (arg1.code == 4)), "fused_nn_instance_norm_1: Expect arg[1] to be pointer")
assert(((((arg2.code == 3) || (arg2.code == 13)) || (arg2.code == 7)) || (arg2.code == 4)), "fused_nn_instance_norm_1: Expect arg[2] to be pointer")
assert(((((arg3.code == 3) || (arg3.code == 13)) || (arg3.code == 7)) || (arg3.code == 4)), "fused_nn_instance_norm_1: Expect arg[3] to be pointer")
// attr ["default"] device_context_id = dev_id
// attr ["default"] device_context_type = dev_type
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)2) && (tvm_struct_get(arg0, 0, 6) == (uint8)16)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be float16")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)2) && (tvm_struct_get(arg0, 0, 6) == (uint8)16)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be float16")
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((int32(arg0.shape[0]) == 1), "Argument arg0.shape[0] has an unsatisfied constraint")
assert((int32(arg0.shape[1]) == 384), "Argument arg0.shape[1] has an unsatisfied constraint")
assert((int32(arg0.shape[2]) == 16), "Argument arg0.shape[2] has an unsatisfied constraint")
assert((int32(arg0.shape[3]) == 8), "Argument arg0.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg0, 0, 8) == (uint64)0), "Argument arg0.byte_offset has an unsatisfied constraint")
assert((1 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 1")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)2) && (tvm_struct_get(arg1, 0, 6) == (uint8)16)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be float16")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)2) && (tvm_struct_get(arg1, 0, 6) == (uint8)16)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be float16")
assert((1 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 1")
assert((int32(arg1.shape[0]) == 384), "Argument arg1.shape[0] has an unsatisfied constraint")
assert((tvm_struct_get(arg1, 0, 8) == (uint64)0), "Argument arg1.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg1, 0, 10)), "Argument arg1.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg1, 0, 9)), "Argument arg1.device_id has an unsatisfied constraint")
assert((1 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 1")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)2) && (tvm_struct_get(arg2, 0, 6) == (uint8)16)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be float16")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)2) && (tvm_struct_get(arg2, 0, 6) == (uint8)16)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be float16")
assert((1 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 1")
assert((int32(arg2.shape[0]) == 384), "Argument arg2.shape[0] has an unsatisfied constraint")
assert((tvm_struct_get(arg2, 0, 8) == (uint64)0), "Argument arg2.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg2, 0, 10)), "Argument arg2.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg2, 0, 9)), "Argument arg2.device_id has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg3, 0, 4)), "arg3.ndim is expected to equal 4")
assert((((tvm_struct_get(arg3, 0, 5) == (uint8)2) && (tvm_struct_get(arg3, 0, 6) == (uint8)16)) && (tvm_struct_get(arg3, 0, 7) == (uint16)1)), "arg3.dtype is expected to be float16")
assert((((tvm_struct_get(arg3, 0, 5) == (uint8)2) && (tvm_struct_get(arg3, 0, 6) == (uint8)16)) && (tvm_struct_get(arg3, 0, 7) == (uint16)1)), "arg3.dtype is expected to be float16")
assert((4 == tvm_struct_get(arg3, 0, 4)), "arg3.ndim is expected to equal 4")
assert((int32(arg3.shape[0]) == 1), "Argument arg3.shape[0] has an unsatisfied constraint")
assert((int32(arg3.shape[1]) == 384), "Argument arg3.shape[1] has an unsatisfied constraint")
assert((int32(arg3.shape[2]) == 16), "Argument arg3.shape[2] has an unsatisfied constraint")
assert((int32(arg3.shape[3]) == 8), "Argument arg3.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg3, 0, 8) == (uint64)0), "Argument arg3.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg3, 0, 10)), "Argument arg3.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg3, 0, 9)), "Argument arg3.device_id has an unsatisfied constraint")
if ((dev_type != 1)) {
  tvm_call_packed("__tvm_set_device", dev_type, dev_id)
}
// attr [0] compute_scope = "fused_nn_instance_norm_1_compute_"
// attr [T_cast] storage_scope = "global"
allocate T_cast[float32 * 49152]
// attr [T_divide] storage_scope = "global"
allocate T_divide[float32 * 49152]
// attr [T_sum] storage_scope = "global"
allocate T_sum[float32 * 384]
produce T_cast {
  for (ax1, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_cast[((((ax1*16) + ax2)*8) + ax3)] = float32(placeholder[((((ax1*16) + ax2)*8) + ax3)])
      }
    }
  }
}
produce T_divide {
  for (ax1, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_divide[((((ax1*16) + ax2)*8) + ax3)] = (T_cast[((((ax1*16) + ax2)*8) + ax3)]*0.007812f)
      }
    }
  }
}
produce T_sum {
  for (ax1, 0, 384) {
    T_sum[ax1] = 0.000000f
    for (k2, 0, 16) {
      for (k3, 0, 8) {
        T_sum[ax1] = (T_sum[ax1] + T_divide[((((ax1*16) + k2)*8) + k3)])
      }
    }
  }
}
produce T_subtract {
  for (ax1, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_cast[((((ax1*16) + ax2)*8) + ax3)] = (T_cast[((((ax1*16) + ax2)*8) + ax3)] - T_sum[ax1])
      }
    }
  }
}
produce T_power {
  for (ax1, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_divide[((((ax1*16) + ax2)*8) + ax3)] = pow(T_cast[((((ax1*16) + ax2)*8) + ax3)], 2.000000f)
      }
    }
  }
}
produce T_sum {
  for (ax1, 0, 384) {
    T_sum[ax1] = 0.000000f
    for (k2, 0, 16) {
      for (k3, 0, 8) {
        T_sum[ax1] = (T_sum[ax1] + T_divide[((((ax1*16) + k2)*8) + k3)])
      }
    }
  }
}
produce T_divide {
  for (ax1, 0, 384) {
    T_sum[ax1] = (T_sum[ax1]*0.007812f)
  }
}
produce T_add {
  for (ax1, 0, 384) {
    T_sum[ax1] = (T_sum[ax1] + 0.000010f)
  }
}
produce T_sqrt {
  for (ax1, 0, 384) {
    T_sum[ax1] = sqrt(T_sum[ax1])
  }
}
produce T_divide {
  for (ax1, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_cast[((((ax1*16) + ax2)*8) + ax3)] = (T_cast[((((ax1*16) + ax2)*8) + ax3)]/T_sum[ax1])
      }
    }
  }
}
produce T_cast {
  for (ax1, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_divide[((((ax1*16) + ax2)*8) + ax3)] = float16(T_cast[((((ax1*16) + ax2)*8) + ax3)])
      }
    }
  }
}
produce T_expand_dims {
  for (ax0, 0, 384) {
    T_sum[ax0] = placeholder[ax0]
  }
}
produce T_multiply {
  for (ax1, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_cast[((((ax1*16) + ax2)*8) + ax3)] = (T_divide[((((ax1*16) + ax2)*8) + ax3)]*T_sum[ax1])
      }
    }
  }
}
produce T_expand_dims {
  for (ax0, 0, 384) {
    T_sum[ax0] = placeholder[ax0]
  }
}
produce T_add {
  for (ax1, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_add[((((ax1*16) + ax2)*8) + ax3)] = (T_cast[((((ax1*16) + ax2)*8) + ax3)] + T_sum[ax1])
      }
    }
  }
}
