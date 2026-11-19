"""s7_binview — net.bin 二进制结构解析(纯分析,无厂商调用)。

布局(依据 deepeye/python/deepeye/nnp3_vu/nnp3_vu_block.py 的打包代码;
详见 docs/03_backend.md §3.2):

  [0  :128] model_name (ASCII)
  [128:152] 创建时间 "YYYY-MM-DD HH:MM:SS"
  [152:156] net_header_len   = 384
  [156:158] block_header_len = 48
  [158:160] layer_header_len = 64
  [160:224] magic: "intellif-<TARGET>-ISA"     ← 硬件目标识别
  [224:312] '@18ILL'(88B,含对齐):
            block_num, net_bin_size, ddr_size(33bit!), net_weight_size,
            TVM 版本两半字, arm_so_size, model_inputs_num, ..., model_info_offset,
            internal_ver[3], lst_file_size, weight_reorder_file_size
  [384:...] blocks:每块头 48B('4H4IH'+保留):blockId/layerNum/type
            (0=NNP,1=CRGH+NNP,2=ARM,3=CRGH,4=MODEL_OUT)/blockSize/innerbuf(33bit DDR)
            每层头 64B:tensor 数、权重地址(model.bin 内偏移)、代码地址、融合算子数...
  尾部      model_info JSON(compile_info/输入输出张量描述)

combine 模式: [net.bin][model.bin][libarm.so][lst][weight_reorder] → net_combine.bin
"""
import os
import struct
import json

from stages.common import Ctx, stage_banner, ensure, stage_dir

BLOCK_TYPE = {0: "NNP", 1: "CRGH+NNP", 2: "ARM", 3: "CRGH", 4: "MODEL_OUT"}


def parse_net_bin(path):
    data = open(path, "rb").read()
    info = {"file": path, "size": len(data)}
    info["model_name"] = data[:128].rstrip(b"\x00").decode(errors="replace")
    info["build_time"] = data[128:152].rstrip(b"\x00").decode(errors="replace").strip()
    (info["net_header_len"], info["block_header_len"], info["layer_header_len"]) = \
        struct.unpack_from("@IHH", data, 152)
    info["magic"] = data[160:224].rstrip(b"\x00").decode(errors="replace")
    (block_num, net_bin_size, ddr_lo, ddr_hi, w_lo, w_hi, ver0, ver1,
     arm_so, model_inputs) = struct.unpack_from("@10I", data, 224)
    vals = struct.unpack_from("@18ILL", data, 224)
    info.update(block_num=vals[0], net_bin_size=vals[1],
                ddr_size=vals[3] * (1 << 32) + vals[2],
                net_weight_size=vals[5] * (1 << 32) + vals[4],
                tvm_ver_raw=(vals[6], vals[7]), arm_so_size=vals[8],
                model_inputs=vals[9], model_info_offset=vals[14])

    blocks, off = [], info["net_header_len"]
    for _ in range(info.get("block_num", 0)):
        if off + 48 > len(data):
            break
        h4, i4 = struct.unpack_from("@4H", data, off), struct.unpack_from("@4I", data, off + 8)
        b = dict(blockId=h4[0], layerNum=h4[1], type=BLOCK_TYPE.get(h4[2], h4[2]),
                 blockSize=i4[0], innerbuf_size=i4[1],
                 innerbuf_addr=i4[3] * (1 << 32) + i4[2] if (i4[2] or i4[3]) else 0)
        size = i4[1] if i4[1] else 48
        blocks.append(b)
        off += size
    info["blocks"] = blocks
    return info


def run(ctx: Ctx = None) -> Ctx:
    if ctx is None:
        ctx = Ctx()
    # 磁盘回退:net.bin 已在则直接用,不触发上游重算
    nd = ctx.get("netbin_dir")
    if not (nd and os.path.isfile(os.path.join(nd, "net.bin"))):
        cand = os.path.join(stage_dir("s6_netbin"), "net.bin")
        if os.path.isfile(cand):
            nd = os.path.dirname(cand)
        else:
            nd = ensure(ctx, "s6_netbin", "netbin_dir")
    stage_banner("s7_binview net.bin 结构解析")
    d = stage_dir("s7_binview")

    lines = []
    for name in ("net.bin", "model.bin", "net_combine.bin", "libarm.so",
                 "nnp_hex_info_0xb0000000.bin"):
        p = os.path.join(nd, name)
        if os.path.isfile(p):
            lines.append(f"{name:32s} {os.path.getsize(p):>10,} B")

    info = parse_net_bin(os.path.join(nd, "net.bin"))
    lines.append("")
    lines.append(f"model_name   : {info['model_name']}")
    lines.append(f"build_time   : {info['build_time']}")
    lines.append(f"magic        : {info['magic']}")
    lines.append(f"headers      : net={info['net_header_len']} block={info['block_header_len']}"
                 f" layer={info['layer_header_len']}")
    lines.append(f"blocks       : {info['block_num']}, net_bin_size={info['net_bin_size']:,}")
    lines.append(f"ddr_size     : {info['ddr_size']:,} B  (全模型张量缓冲需求,33bit)")
    lines.append(f"weight_size  : {info['net_weight_size']:,} B  (应≈model.bin)")
    v0, v1 = info.get("tvm_ver_raw", (0, 0))
    lines.append(f"tvm_ver      : raw=0x{v0:x}/0x{v1:x}(位域编码)"
                 f"  arm_so: {info['arm_so_size']:,} B  inputs: {info['model_inputs']}")
    lines.append("")
    lines.append(f"{'blockId':>7} {'type':>10} {'layers':>6} {'blockSize':>12} {'innerbuf':>12}")
    for b in info["blocks"]:
        lines.append(f"{b['blockId']:>7} {b['type']:>10} {b['layerNum']:>6} "
                     f"{b['blockSize']:>12,} {b['innerbuf_size']:>12,}")

    # 尾部 model_info JSON
    try:
        tail = open(os.path.join(nd, "net.bin"), "rb").read()
        js = json.loads(tail[info["model_info_offset"]:].rstrip(b"\x00 "))
        with open(os.path.join(d, "model_info.json"), "w") as f:
            json.dump(js, f, indent=2, ensure_ascii=False)
        lines.append(f"model_info   : 已提取 → model_info.json")
    except Exception as e:
        lines.append(f"model_info   : 提取失败({e})")

    report = "\n".join(lines)
    print(report)
    with open(os.path.join(d, "net_bin_layout.txt"), "w") as f:
        f.write(report + "\n")
    ctx.netbin_info = info
    m = ctx.get("metrics", {})
    m.update(net_bin_size=info["net_bin_size"], ddr_size=info["ddr_size"],
             weight_size=info["net_weight_size"])
    ctx.metrics = m
    return ctx


if __name__ == "__main__":
    run()
