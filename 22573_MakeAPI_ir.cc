assert((num_args == 3), "fused_nn_conv2d_29: num_args should be 3")
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
  assert(((((1 == int32(arg0.strides[3])) && (30 == int32(arg0.strides[2]))) && (480 == int32(arg0.strides[1]))) && (30720 == int32(arg0.strides[0]))), "arg0.strides: expected to be compact array")
  0
}
let dev_type = tvm_struct_get(arg0, 0, 10)
let dev_id = tvm_struct_get(arg0, 0, 9)
let placeholder = tvm_struct_get(arg1, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg1.shape = tvm_struct_get(arg1, 0, 2)
let arg1.strides = tvm_struct_get(arg1, 0, 3)
if (!tvm_handle_is_null(arg1.strides)) {
  assert(((((1 == int32(arg1.strides[3])) && (1 == int32(arg1.strides[2]))) && (1 == int32(arg1.strides[1]))) && (64 == int32(arg1.strides[0]))), "arg1.strides: expected to be compact array")
  0
}
let output_unpack = tvm_struct_get(arg2, 0, 1)
// attr [output_unpack] storage_alignment = 64
let arg2.shape = tvm_struct_get(arg2, 0, 2)
let arg2.strides = tvm_struct_get(arg2, 0, 3)
if (!tvm_handle_is_null(arg2.strides)) {
  assert(((((1 == int32(arg2.strides[3])) && (30 == int32(arg2.strides[2]))) && (480 == int32(arg2.strides[1]))) && (480 == int32(arg2.strides[0]))), "arg2.strides: expected to be compact array")
  0
}
assert(((((arg0.code == 3) || (arg0.code == 13)) || (arg0.code == 7)) || (arg0.code == 4)), "fused_nn_conv2d_29: Expect arg[0] to be pointer")
assert(((((arg1.code == 3) || (arg1.code == 13)) || (arg1.code == 7)) || (arg1.code == 4)), "fused_nn_conv2d_29: Expect arg[1] to be pointer")
assert(((((arg2.code == 3) || (arg2.code == 13)) || (arg2.code == 7)) || (arg2.code == 4)), "fused_nn_conv2d_29: Expect arg[2] to be pointer")
// attr ["default"] device_context_id = dev_id
// attr ["default"] device_context_type = dev_type
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((int32(arg0.shape[0]) == 1), "Argument arg0.shape[0] has an unsatisfied constraint")
assert((int32(arg0.shape[1]) == 64), "Argument arg0.shape[1] has an unsatisfied constraint")
assert((int32(arg0.shape[2]) == 16), "Argument arg0.shape[2] has an unsatisfied constraint")
assert((int32(arg0.shape[3]) == 30), "Argument arg0.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg0, 0, 8) == (uint64)0), "Argument arg0.byte_offset has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((int32(arg1.shape[0]) == 1), "Argument arg1.shape[0] has an unsatisfied constraint")
assert((int32(arg1.shape[1]) == 64), "Argument arg1.shape[1] has an unsatisfied constraint")
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
assert((int32(arg2.shape[1]) == 1), "Argument arg2.shape[1] has an unsatisfied constraint")
assert((int32(arg2.shape[2]) == 16), "Argument arg2.shape[2] has an unsatisfied constraint")
assert((int32(arg2.shape[3]) == 30), "Argument arg2.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg2, 0, 8) == (uint64)0), "Argument arg2.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg2, 0, 10)), "Argument arg2.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg2, 0, 9)), "Argument arg2.device_id has an unsatisfied constraint")
if ((dev_type != 1)) {
  tvm_call_packed("__tvm_set_device", dev_type, dev_id)
}
// attr [0] compute_scope = "fused_nn_conv2d_29_compute_"
// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 30720]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8 * 64]
produce data_vec {
  parallel (C.h.fused, 0, 1024) {
    for (w, 0, 30) {
      data_vec[((C.h.fused*30) + w)] = placeholder[((C.h.fused*30) + w)]
    }
  }
}
produce kernel_vec {
  for (CI, 0, 64) {
    kernel_vec[CI] = placeholder[CI]
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 16) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64 * 30]
    produce conv {
      produce conv.global {
        conv.global[0] = (int64)0
        conv.global[1] = (int64)0
        conv.global[2] = (int64)0
        conv.global[3] = (int64)0
        conv.global[4] = (int64)0
        conv.global[5] = (int64)0
        conv.global[6] = (int64)0
        conv.global[7] = (int64)0
        conv.global[8] = (int64)0
        conv.global[9] = (int64)0
        conv.global[10] = (int64)0
        conv.global[11] = (int64)0
        conv.global[12] = (int64)0
        conv.global[13] = (int64)0
        conv.global[14] = (int64)0
        conv.global[15] = (int64)0
        conv.global[16] = (int64)0
        conv.global[17] = (int64)0
        conv.global[18] = (int64)0
        conv.global[19] = (int64)0
        conv.global[20] = (int64)0
        conv.global[21] = (int64)0
        conv.global[22] = (int64)0
        conv.global[23] = (int64)0
        conv.global[24] = (int64)0
        conv.global[25] = (int64)0
        conv.global[26] = (int64)0
        conv.global[27] = (int64)0
        conv.global[28] = (int64)0
        conv.global[29] = (int64)0
        for (ic.outer, 0, 64) {
          conv.global[0] = (conv.global[0] + (int64(data_vec[(((ic.outer*16) + c.outer.h.outer.fused)*30)])*int64(kernel_vec[ic.outer])))
          conv.global[1] = (conv.global[1] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 1)])*int64(kernel_vec[ic.outer])))
          conv.global[2] = (conv.global[2] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 2)])*int64(kernel_vec[ic.outer])))
          conv.global[3] = (conv.global[3] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 3)])*int64(kernel_vec[ic.outer])))
          conv.global[4] = (conv.global[4] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 4)])*int64(kernel_vec[ic.outer])))
          conv.global[5] = (conv.global[5] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 5)])*int64(kernel_vec[ic.outer])))
          conv.global[6] = (conv.global[6] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 6)])*int64(kernel_vec[ic.outer])))
          conv.global[7] = (conv.global[7] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 7)])*int64(kernel_vec[ic.outer])))
          conv.global[8] = (conv.global[8] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 8)])*int64(kernel_vec[ic.outer])))
          conv.global[9] = (conv.global[9] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 9)])*int64(kernel_vec[ic.outer])))
          conv.global[10] = (conv.global[10] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 10)])*int64(kernel_vec[ic.outer])))
          conv.global[11] = (conv.global[11] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 11)])*int64(kernel_vec[ic.outer])))
          conv.global[12] = (conv.global[12] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 12)])*int64(kernel_vec[ic.outer])))
          conv.global[13] = (conv.global[13] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 13)])*int64(kernel_vec[ic.outer])))
          conv.global[14] = (conv.global[14] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 14)])*int64(kernel_vec[ic.outer])))
          conv.global[15] = (conv.global[15] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 15)])*int64(kernel_vec[ic.outer])))
          conv.global[16] = (conv.global[16] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 16)])*int64(kernel_vec[ic.outer])))
          conv.global[17] = (conv.global[17] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 17)])*int64(kernel_vec[ic.outer])))
          conv.global[18] = (conv.global[18] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 18)])*int64(kernel_vec[ic.outer])))
          conv.global[19] = (conv.global[19] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 19)])*int64(kernel_vec[ic.outer])))
          conv.global[20] = (conv.global[20] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 20)])*int64(kernel_vec[ic.outer])))
          conv.global[21] = (conv.global[21] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 21)])*int64(kernel_vec[ic.outer])))
          conv.global[22] = (conv.global[22] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 22)])*int64(kernel_vec[ic.outer])))
          conv.global[23] = (conv.global[23] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 23)])*int64(kernel_vec[ic.outer])))
          conv.global[24] = (conv.global[24] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 24)])*int64(kernel_vec[ic.outer])))
          conv.global[25] = (conv.global[25] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 25)])*int64(kernel_vec[ic.outer])))
          conv.global[26] = (conv.global[26] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 26)])*int64(kernel_vec[ic.outer])))
          conv.global[27] = (conv.global[27] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 27)])*int64(kernel_vec[ic.outer])))
          conv.global[28] = (conv.global[28] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 28)])*int64(kernel_vec[ic.outer])))
          conv.global[29] = (conv.global[29] + (int64(data_vec[((((ic.outer*16) + c.outer.h.outer.fused)*30) + 29)])*int64(kernel_vec[ic.outer])))
        }
      }
    }
    for (w.inner, 0, 30) {
      output_unpack[((c.outer.h.outer.fused*30) + w.inner)] = conv.global[w.inner]
    }
  }
}
