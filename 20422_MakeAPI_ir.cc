assert((num_args == 3), "fused_nn_conv2d_8: num_args should be 3")
let arg0 = tvm_struct_get(args, 0, 12)
let arg0.code = arg_type_ids[0]
let arg1 = tvm_struct_get(args, 1, 12)
let arg1.code = arg_type_ids[1]
let arg2 = tvm_struct_get(args, 2, 12)
let arg2.code = arg_type_ids[2]
let placeholder = tvm_struct_get(arg0, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg0.shape = tvm_struct_get(arg0, 0, 2)
let arg0.strides = tvm_struct_get(arg0, 0, 3)
if (!tvm_handle_is_null(arg0.strides)) {
  assert(((((1 == int32(arg0.strides[3])) && (15 == int32(arg0.strides[2]))) && (120 == int32(arg0.strides[1]))) && (30720 == int32(arg0.strides[0]))), "arg0.strides: expected to be compact array")
  0
}
let dev_type = tvm_struct_get(arg0, 0, 10)
let dev_id = tvm_struct_get(arg0, 0, 9)
let placeholder = tvm_struct_get(arg1, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg1.shape = tvm_struct_get(arg1, 0, 2)
let arg1.strides = tvm_struct_get(arg1, 0, 3)
if (!tvm_handle_is_null(arg1.strides)) {
  assert(((((1 == int32(arg1.strides[3])) && (1 == int32(arg1.strides[2]))) && (1 == int32(arg1.strides[1]))) && (256 == int32(arg1.strides[0]))), "arg1.strides: expected to be compact array")
  0
}
let output_unpack = tvm_struct_get(arg2, 0, 1)
// attr [output_unpack] storage_alignment = 64
let arg2.shape = tvm_struct_get(arg2, 0, 2)
let arg2.strides = tvm_struct_get(arg2, 0, 3)
if (!tvm_handle_is_null(arg2.strides)) {
  assert(((((1 == int32(arg2.strides[3])) && (15 == int32(arg2.strides[2]))) && (120 == int32(arg2.strides[1]))) && (15360 == int32(arg2.strides[0]))), "arg2.strides: expected to be compact array")
  0
}
assert(((((arg0.code == 3) || (arg0.code == 13)) || (arg0.code == 7)) || (arg0.code == 4)), "fused_nn_conv2d_8: Expect arg[0] to be pointer")
assert(((((arg1.code == 3) || (arg1.code == 13)) || (arg1.code == 7)) || (arg1.code == 4)), "fused_nn_conv2d_8: Expect arg[1] to be pointer")
assert(((((arg2.code == 3) || (arg2.code == 13)) || (arg2.code == 7)) || (arg2.code == 4)), "fused_nn_conv2d_8: Expect arg[2] to be pointer")
// attr ["default"] device_context_id = dev_id
// attr ["default"] device_context_type = dev_type
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((int32(arg0.shape[0]) == 1), "Argument arg0.shape[0] has an unsatisfied constraint")
assert((int32(arg0.shape[1]) == 256), "Argument arg0.shape[1] has an unsatisfied constraint")
assert((int32(arg0.shape[2]) == 8), "Argument arg0.shape[2] has an unsatisfied constraint")
assert((int32(arg0.shape[3]) == 15), "Argument arg0.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg0, 0, 8) == (uint64)0), "Argument arg0.byte_offset has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((int32(arg1.shape[0]) == 128), "Argument arg1.shape[0] has an unsatisfied constraint")
assert((int32(arg1.shape[1]) == 256), "Argument arg1.shape[1] has an unsatisfied constraint")
assert((int32(arg1.shape[2]) == 1), "Argument arg1.shape[2] has an unsatisfied constraint")
assert((int32(arg1.shape[3]) == 1), "Argument arg1.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg1, 0, 8) == (uint64)0), "Argument arg1.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg1, 0, 10)), "Argument arg1.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg1, 0, 9)), "Argument arg1.device_id has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 4")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)0) && (tvm_struct_get(arg2, 0, 6) == (uint8)64)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be int64")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)0) && (tvm_struct_get(arg2, 0, 6) == (uint8)64)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be int64")
assert((4 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 4")
assert((int32(arg2.shape[0]) == 1), "Argument arg2.shape[0] has an unsatisfied constraint")
assert((int32(arg2.shape[1]) == 128), "Argument arg2.shape[1] has an unsatisfied constraint")
assert((int32(arg2.shape[2]) == 8), "Argument arg2.shape[2] has an unsatisfied constraint")
assert((int32(arg2.shape[3]) == 15), "Argument arg2.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg2, 0, 8) == (uint64)0), "Argument arg2.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg2, 0, 10)), "Argument arg2.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg2, 0, 9)), "Argument arg2.device_id has an unsatisfied constraint")
if ((dev_type != 1)) {
  tvm_call_packed("__tvm_set_device", dev_type, dev_id)
}
// attr [0] compute_scope = "fused_nn_conv2d_8_compute_"
// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 30720]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 4096]
produce data_vec {
  parallel (C.h.fused, 0, 256) {
    for (c, 0, 8) {
      for (w, 0, 15) {
        data_vec[((((C.h.fused*8) + c)*15) + w)] = placeholder[(((((((C.h.fused/8)*8) + c)*8) + (C.h.fused % 8))*15) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 16) {
    for (CI, 0, 32) {
      for (ci, 0, 8) {
        kernel_vec[ramp((((((CO.h.fused*32) + CI)*8) + ci)*8), 1, 8)] = placeholder[ramp(((((CO.h.fused*256) + CI)*8) + ci), 256, 8)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 64) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
    produce conv {
      produce conv.global {
        conv.global[ramp(0, 1, 8)] = x8((int64)0)
        conv.global[ramp(8, 1, 8)] = x8((int64)0)
        conv.global[ramp(16, 1, 8)] = x8((int64)0)
        conv.global[ramp(24, 1, 8)] = x8((int64)0)
        conv.global[ramp(32, 1, 8)] = x8((int64)0)
        conv.global[ramp(40, 1, 8)] = x8((int64)0)
        conv.global[ramp(48, 1, 8)] = x8((int64)0)
        conv.global[ramp(56, 1, 8)] = x8((int64)0)
        conv.global[ramp(64, 1, 8)] = x8((int64)0)
        conv.global[ramp(72, 1, 8)] = x8((int64)0)
        conv.global[ramp(80, 1, 8)] = x8((int64)0)
        conv.global[ramp(88, 1, 8)] = x8((int64)0)
        conv.global[ramp(96, 1, 8)] = x8((int64)0)
        conv.global[ramp(104, 1, 8)] = x8((int64)0)
        conv.global[ramp(112, 1, 8)] = x8((int64)0)
        conv.global[ramp(120, 1, 8)] = x8((int64)0)
        conv.global[ramp(128, 1, 8)] = x8((int64)0)
        conv.global[ramp(136, 1, 8)] = x8((int64)0)
        conv.global[ramp(144, 1, 8)] = x8((int64)0)
        conv.global[ramp(152, 1, 8)] = x8((int64)0)
        conv.global[ramp(160, 1, 8)] = x8((int64)0)
        conv.global[ramp(168, 1, 8)] = x8((int64)0)
        conv.global[ramp(176, 1, 8)] = x8((int64)0)
        conv.global[ramp(184, 1, 8)] = x8((int64)0)
        conv.global[ramp(192, 1, 8)] = x8((int64)0)
        conv.global[ramp(200, 1, 8)] = x8((int64)0)
        conv.global[ramp(208, 1, 8)] = x8((int64)0)
        conv.global[ramp(216, 1, 8)] = x8((int64)0)
        conv.global[ramp(224, 1, 8)] = x8((int64)0)
        conv.global[ramp(232, 1, 8)] = x8((int64)0)
        for (ic.outer, 0, 32) {
          for (ic.inner, 0, 8) {
            conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[(((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 1)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 2)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 3)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 4)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 5)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 6)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 7)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 8)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 9)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 10)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 11)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 12)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 13)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 14)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 120)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(128, 1, 8)] = (conv.global[ramp(128, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 121)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(136, 1, 8)] = (conv.global[ramp(136, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 122)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(144, 1, 8)] = (conv.global[ramp(144, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 123)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(152, 1, 8)] = (conv.global[ramp(152, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 124)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(160, 1, 8)] = (conv.global[ramp(160, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 125)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(168, 1, 8)] = (conv.global[ramp(168, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 126)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(176, 1, 8)] = (conv.global[ramp(176, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 127)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(184, 1, 8)] = (conv.global[ramp(184, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 128)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(192, 1, 8)] = (conv.global[ramp(192, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 129)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(200, 1, 8)] = (conv.global[ramp(200, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 130)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(208, 1, 8)] = (conv.global[ramp(208, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 131)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(216, 1, 8)] = (conv.global[ramp(216, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 132)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(224, 1, 8)] = (conv.global[ramp(224, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 133)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
            conv.global[ramp(232, 1, 8)] = (conv.global[ramp(232, 1, 8)] + (x8(int64(data_vec[((((((ic.outer*4) + (c.outer.h.outer.fused % 4))*16) + ic.inner)*15) + 134)]))*int64x8(kernel_vec[ramp(((((((c.outer.h.outer.fused/4)*32) + ic.outer)*8) + ic.inner)*8), 1, 8)])))
          }
        }
      }
    }
    for (h.inner, 0, 2) {
      for (w.inner, 0, 15) {
        output_unpack[ramp((((((((c.outer.h.outer.fused/4)*32) + (c.outer.h.outer.fused % 4))*2) + h.inner)*15) + w.inner), 120, 8)] = conv.global[ramp((((h.inner*15) + w.inner)*8), 1, 8)]
      }
    }
  }
}
