"""s2_quantize — 校准 + int8 量化(Relay 浮点图 → 已实化 int8 图)。

厂商对应代码(核心 IP,git 子模块):
  - python/tvm/relay/quantization/quantize.py:184  quantize() 入口
    五步流水线:
      1. analyze_graph   analyze.py:741   每个 Call 生成 VertexConfig(量不量化/per-channel axis/dtype)
      2. collect_stats   collect.py:342   校准数据收集:LLVM graph_runtime 在 CPU 跑 float 模型,
                                          逐 node 记录 min/max(本 stage 用伪随机帧)
      3. calibrate_params calibrate.py:334 逐节点定 scale/zero_point(对称量化,zero_point=0;
                                          权重 per-channel,激活 per-tensor;算法默认 l2norm,
                                          可选 kld/percentile/mse,threshold.py:1623)
      4. realize_graph   realize.py:1347   实化:Q 算子 → cast(fp16)→multiply(1/scale)→round→
                                          clip→cast(int8);requantize → multiply + right_shift 链
                                          (匹配 NPU 语义:activation 24bit / bias 24bit)
      5. post_process    post_process.py:29 remove_and_fuse / FoldConstantQuantize
  - get_quantize_config: tools.py:89(nbit_in/w=8, bias/act=24, perchannel=True, pow2scale=False)

修改实验点:换 calib_method(kld/mse/percentile)、改 nbit、per-channel 开关 —— 都在
get_quantize_config 的返回值里改,或直接改 tools.py:99-139 默认表。
"""
import os

from stages.common import Ctx, stage_banner, stage_dir, ensure, INPUT_NAME, TARGET


def run(ctx: Ctx = None) -> Ctx:
    if ctx is None:
        ctx = Ctx()
    sym = ensure(ctx, "s1_front_end", "relay_fp")
    calib = ensure(ctx, "s0_model", "calib_frames")
    target = TARGET
    _names = list(calib)
    stage_banner(f"s2_quantize 校准+int8 量化({len(calib[_names[0]])} 帧伪随机校准数据)")

    from tvm import relay
    from tvm.relay.quantization import quantize

    quantize_config = relay.quantization.get_quantize_config(
        target, {n: "uint8" for n in calib})     # 硬件输入一律按 uint8 摄像头流水线
    d = stage_dir("s2_quantize")

    def dataset():
        # 多输入:每帧是 {input_name: array};帧数取第一个输入的长度
        names = list(calib)
        n = len(calib[names[0]])
        return iter({k: calib[k][i] for k in names} for i in range(n))

    names = list(calib)
    n_frames = len(calib[names[0]])
    relay_func = quantize(sym, None,
                          dataset=dataset,
                          prof_img_num=n_frames,   # 自定义 dataset 必须显式给帧数
                          rgb_en=0,            # numpy 直喂,不做图像 RGB/通道预处理
                          norm=None,
                          quantize_config=quantize_config,
                          debug_level=0,
                          save_dir=d)

    with open(os.path.join(d, "relay_int8.txt"), "w") as f:
        f.write(relay_func.astext(show_meta_data=False))
    print(f"  量化后 IR → {d}/relay_int8.txt")
    print(f"  (cast/multiply/round/clip/right_shift 链 = NPU 量化语义;权重已烘成 int 常量)")
    ctx.relay_int8 = relay_func
    return ctx


if __name__ == "__main__":
    run()
