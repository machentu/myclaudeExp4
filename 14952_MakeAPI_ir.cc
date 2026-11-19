assert((num_args == 3), "fused_nn_conv2d_17: num_args should be 3")
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
  assert(((((1 == int32(arg0.strides[3])) && (16 == int32(arg0.strides[2]))) && (512 == int32(arg0.strides[1]))) && (32768 == int32(arg0.strides[0]))), "arg0.strides: expected to be compact array")
  0
}
let dev_type = tvm_struct_get(arg0, 0, 10)
let dev_id = tvm_struct_get(arg0, 0, 9)
let placeholder = tvm_struct_get(arg1, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg1.shape = tvm_struct_get(arg1, 0, 2)
let arg1.strides = tvm_struct_get(arg1, 0, 3)
if (!tvm_handle_is_null(arg1.strides)) {
  assert(((((1 == int32(arg1.strides[3])) && (3 == int32(arg1.strides[2]))) && (9 == int32(arg1.strides[1]))) && (9 == int32(arg1.strides[0]))), "arg1.strides: expected to be compact array")
  0
}
let DepthwiseConv2d = tvm_struct_get(arg2, 0, 1)
// attr [DepthwiseConv2d] storage_alignment = 64
let arg2.shape = tvm_struct_get(arg2, 0, 2)
let arg2.strides = tvm_struct_get(arg2, 0, 3)
if (!tvm_handle_is_null(arg2.strides)) {
  assert(((((1 == int32(arg2.strides[3])) && (16 == int32(arg2.strides[2]))) && (512 == int32(arg2.strides[1]))) && (32768 == int32(arg2.strides[0]))), "arg2.strides: expected to be compact array")
  0
}
assert(((((arg0.code == 3) || (arg0.code == 13)) || (arg0.code == 7)) || (arg0.code == 4)), "fused_nn_conv2d_17: Expect arg[0] to be pointer")
assert(((((arg1.code == 3) || (arg1.code == 13)) || (arg1.code == 7)) || (arg1.code == 4)), "fused_nn_conv2d_17: Expect arg[1] to be pointer")
assert(((((arg2.code == 3) || (arg2.code == 13)) || (arg2.code == 7)) || (arg2.code == 4)), "fused_nn_conv2d_17: Expect arg[2] to be pointer")
// attr ["default"] device_context_id = dev_id
// attr ["default"] device_context_type = dev_type
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((int32(arg0.shape[0]) == 1), "Argument arg0.shape[0] has an unsatisfied constraint")
assert((int32(arg0.shape[1]) == 64), "Argument arg0.shape[1] has an unsatisfied constraint")
assert((int32(arg0.shape[2]) == 32), "Argument arg0.shape[2] has an unsatisfied constraint")
assert((int32(arg0.shape[3]) == 16), "Argument arg0.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg0, 0, 8) == (uint64)0), "Argument arg0.byte_offset has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((int32(arg1.shape[0]) == 64), "Argument arg1.shape[0] has an unsatisfied constraint")
assert((int32(arg1.shape[1]) == 1), "Argument arg1.shape[1] has an unsatisfied constraint")
assert((int32(arg1.shape[2]) == 3), "Argument arg1.shape[2] has an unsatisfied constraint")
assert((int32(arg1.shape[3]) == 3), "Argument arg1.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg1, 0, 8) == (uint64)0), "Argument arg1.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg1, 0, 10)), "Argument arg1.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg1, 0, 9)), "Argument arg1.device_id has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 4")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)0) && (tvm_struct_get(arg2, 0, 6) == (uint8)64)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be int64")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)0) && (tvm_struct_get(arg2, 0, 6) == (uint8)64)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be int64")
assert((4 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 4")
assert((int32(arg2.shape[0]) == 1), "Argument arg2.shape[0] has an unsatisfied constraint")
assert((int32(arg2.shape[1]) == 64), "Argument arg2.shape[1] has an unsatisfied constraint")
assert((int32(arg2.shape[2]) == 32), "Argument arg2.shape[2] has an unsatisfied constraint")
assert((int32(arg2.shape[3]) == 16), "Argument arg2.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg2, 0, 8) == (uint64)0), "Argument arg2.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg2, 0, 10)), "Argument arg2.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg2, 0, 9)), "Argument arg2.device_id has an unsatisfied constraint")
if ((dev_type != 1)) {
  tvm_call_packed("__tvm_set_device", dev_type, dev_id)
}
// attr [0] compute_scope = "fused_nn_conv2d_17_compute_"
// attr [placeholder.global.global] storage_scope = "global"
allocate placeholder.global.global[int8 * 576]
// attr [DepthwiseConv2d.global] storage_scope = "global"
allocate DepthwiseConv2d.global[int64 * 32768]
produce placeholder.global.global {
  parallel (ax0.outer.c, 0, 64) {
    for (ax2.c, 0, 3) {
      for (ax3.c, 0, 3) {
        placeholder.global.global[((((ax0.outer.c*3) + ax2.c)*3) + ax3.c)] = placeholder[((((ax0.outer.c*3) + ax2.c)*3) + ax3.c)]
      }
    }
  }
}
produce DepthwiseConv2d.global {
  parallel (c.outer.c, 0, 64) {
    // attr [PaddedInput.global.global] storage_scope = "global"
    allocate PaddedInput.global.global[int8 * 54]
    for (i.c.outer, 0, 32) {
      produce PaddedInput.global.global {
        for (ax2.c, 0, 3) {
          for (ax3.c, 0, 18) {
            PaddedInput.global.global[((ax2.c*18) + ax3.c)] = tvm_if_then_else((((((1 - i.c.outer) <= ax2.c) && (ax2.c < (33 - i.c.outer))) && (1 <= ax3.c)) && (ax3.c < 17)), placeholder[((((((c.outer.c*32) + ax2.c) + i.c.outer)*16) + ax3.c) + -17)], (int8)0)
          }
        }
      }
      for (j.c.outer, 0, 16) {
        DepthwiseConv2d.global[((((c.outer.c*32) + i.c.outer)*16) + j.c.outer)] = (int64)0
        for (di, 0, 3) {
          for (dj, 0, 3) {
            DepthwiseConv2d.global[((((c.outer.c*32) + i.c.outer)*16) + j.c.outer)] = (DepthwiseConv2d.global[((((c.outer.c*32) + i.c.outer)*16) + j.c.outer)] + (int64(PaddedInput.global.global[(((di*18) + j.c.outer) + dj)])*int64(placeholder.global.global[((((c.outer.c*3) + di)*3) + dj)])))
          }
        }
      }
    }
  }
}
produce DepthwiseConv2d {
  parallel (c, 0, 64) {
    for (i, 0, 32) {
      for (j, 0, 16) {
        DepthwiseConv2d[((((c*32) + i)*16) + j)] = DepthwiseConv2d.global[((((c*32) + i)*16) + j)]
      }
    }
  }
}
