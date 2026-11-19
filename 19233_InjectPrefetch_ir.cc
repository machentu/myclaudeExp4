// attr [extern(yolov8_post_process, 0x37b23910)] realize_scope = ""
realize yolov8_post_process([0, 1], [0, 4], [0, 2520]) {
  produce yolov8_post_process {
    // attr [[buffer(placeholder, 0x38cf2410), Tensor(shape=[1, 4, 2520], op.name=placeholder)]] buffer_bind_scope = tvm_tuple(0, 1, 0, 4, 0, 2520)
    // attr [[buffer(placeholder, 0x2ac6cb70), Tensor(shape=[1, 2, 2520], op.name=placeholder)]] buffer_bind_scope = tvm_tuple(0, 1, 0, 2, 0, 2520)
    // attr [[buffer(placeholder, 0x38a120d0), Tensor(shape=[1, 2, 2520], op.name=placeholder)]] buffer_bind_scope = tvm_tuple(0, 1, 0, 2, 0, 2520)
    // attr [[buffer(placeholder, 0x36206610), Tensor(shape=[1, 2520], op.name=placeholder)]] buffer_bind_scope = tvm_tuple(0, 1, 0, 2520)
    // attr [[buffer(yolov8_post_process, 0x36c43b10), Tensor(shape=[1, 4, 2520], op.name=yolov8_post_process)]] buffer_bind_scope = tvm_tuple(0, 1, 0, 4, 0, 2520)
    // attr [0] extern_scope = 0
    tvm_call_packed("tvm_contrib_yolov8_post_process", tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 4, 2520), 0, 3, 0.000000h, 0), tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 2, 2520), 0, 3, 0.000000h, 0), tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 2, 2520), 0, 3, 0.000000h, 0), tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 2520), 0, 2, 0.000000h, 0), tvm_stack_make_array(yolov8_post_process, tvm_stack_make_shape(1, 4, 2520), 0, 3, 0.000000h, 0))
  }
}
