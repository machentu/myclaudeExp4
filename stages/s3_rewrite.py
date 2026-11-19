"""s3_rewrite — deepeye relay 前处理可视化(~60 个 Python ExprMutator 串行)。

⚠️ 工具链脆弱点(2026-11-18 实测发现):
  rewrite_for_deepeye 与 relay.build 存在隐性参数命名耦合。把它从 build_relay 内部
  拆出来单独调用(rewrite → 单独 relay.build),graph 参数节点会保留原名
  (nn.conv2d_0_arg_1),而 get_params() 返回 pXX 键,lto 阶段 KeyError 崩溃;
  同一进程对同一函数跑两次 rewrite 同样崩(KeyError 'nn.bias_add_1_arg_1')。
  ⇒ 本阶段只做「可视化预览」:对量化图跑一次 rewrite 展示变换效果(Relay AST
  不可变,不影响原对象),真正的 rewrite 仍在 s4 的厂商 build_relay 内部完成。

厂商对应代码:
  - deepeye/relay_pass/deepeye_rewrite.py:5146 rewrite_for_deepeye()
    序:DeleteDedspOnDevice → ExtractParams → MoveRightShiftBeforeRelu →
    OpTransform(算子变形大杂烩) → LUTConvert(sigmoid/tanh→LUT) → SoftmaxConvert →
    ConvertReorgToConv / ConvertUpsamplingToConv / ConvertFCToConv →
    ARMAnnotate ★host/NPU 切分决策 → Convert2D3DTo4D → 收尾清理
  完整 pass 表:docs/02_middle.md

产物: relay_rewritten.txt(预览)、relay_int8.txt 的 diff 可看每类变换效果
"""
import os

from stages.common import Ctx, stage_banner, stage_dir, ensure, TARGET


def run(ctx: Ctx = None) -> Ctx:
    if ctx is None:
        ctx = Ctx()
    func = ensure(ctx, "s2_quantize", "relay_int8")
    target_full = f"nnp300 -mnnp={TARGET}"
    stage_banner("s3_rewrite rewrite_for_deepeye 预览(只读,不影响后续)")

    from deepeye.relay_pass.deepeye_rewrite import rewrite_for_deepeye

    rewritten, _ = rewrite_for_deepeye(func, None, target_full)

    d = stage_dir("s3_rewrite")
    with open(os.path.join(d, "relay_rewritten.txt"), "w") as f:
        f.write(rewritten.astext(show_meta_data=False))
    print(f"  改写预览 → {d}/relay_rewritten.txt")
    print(f"  (sigmoid→lut、dense→conv、on_device 切分标注都在这里看)")
    print(f"  ⚠️ 真正的 rewrite 在 s4 的 build_relay 内部执行(见本文件头注释)")
    return ctx


if __name__ == "__main__":
    run()
