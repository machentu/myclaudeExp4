"""s5_tir — TIR pass dump 索引器(纯分析,无厂商调用)。

把 s4 落盘的 <N>_<pass>_ir.cc 整理成:
  index.md          按流水线顺序的文件索引(pass 名解析:custom<K>_phase<P> → 真名)
  intrinsic_evolution.md  ★ nnp_* intrinsic 参数如何在 phase2 逐步"长胖"
  dma_view.md       DMA 相关 pass 的指令演化(idma/eidma/odma/ewdma/ebdma/wbdma)

custom 编号 = pass 在 build_config_nnp300 pass_list 里的全局序号
(build_module.py:218-304;phase0=7 项,phase1=5,phase2=30,phase3=17)。
名字解析通过临时 mock tvm.build_config 截获 add_lower_pass 得到,不硬编码。
"""
import os
import re
import glob

from stages.common import Ctx, stage_banner, ensure


def capture_pass_list():
    """AST 解析厂商 build_module.py 源码,静态提取 pass_list 的有序 pass 名。
    (不实际调用厂商代码 —— 无 Environment 上下文时 get_env() 会段错误)"""
    import ast
    import tvm  # noqa: F401  确保 import 环境可用
    import deepeye.build_module.build_module as bm
    src = open(bm.__file__, errors="ignore").read()
    tree = ast.parse(src)
    func = next(n for n in ast.walk(tree)
                if isinstance(n, ast.FunctionDef) and n.name == "build_config_nnp300")
    names = []
    for node in ast.walk(func):
        if isinstance(node, ast.Call) and getattr(node.func, "attr", "") == "append":
            entry = node.args[0]
            if isinstance(entry, ast.Tuple):
                for elt in entry.elts[1:]:     # 跳过 phase 数字
                    parts = [p.attr if isinstance(p, ast.Attribute) else "?"
                             for p in ast.walk(elt) if isinstance(p, ast.Attribute)]
                    names.append(parts[-1] if parts else
                                 (elt.id if isinstance(elt, ast.Name) else "?"))
    return names


def run(ctx: Ctx = None) -> Ctx:
    if ctx is None:
        ctx = Ctx()
    build_dir = ensure(ctx, "s4_build", "build_dir")
    stage_banner("s5_tir TIR dump 索引(783 个逐 pass IR 文件)")

    d = os.path.dirname(build_dir.rstrip("/")) + "/s5_tir"
    os.makedirs(d, exist_ok=True)

    pass_names = capture_pass_list()
    custom_re = re.compile(r"custom(\d+)_phase(\d)")

    rows = []
    for path in sorted(glob.glob(os.path.join(build_dir, "*_ir.cc")),
                       key=lambda p: int(os.path.basename(p).split("_")[0])):
        base = os.path.basename(path)[:-6]        # strip .cc? 名为 *_ir.cc → 去 "_ir.cc"
        base = re.sub(r"_ir\.cc$", "", os.path.basename(path))
        m = custom_re.match(base.split("_", 1)[1])
        if m:
            k = int(m.group(1))
            resolved = pass_names[k] if k < len(pass_names) else base
            phase = f"P{m.group(2)}"
        else:
            resolved = base.split("_", 1)[1]
            phase = ""
        rows.append((base, resolved, phase, path))

    # ---- index.md ----
    with open(os.path.join(d, "index.md"), "w") as f:
        f.write("# TIR pass dump 索引\n\n源: s4_build/*_ir.cc,按 lower() 执行顺序。\n")
        f.write("`custom<K>` 已解析为真实 pass 名(pass_list 全局序号 K)。\n\n")
        f.write("| # | 序号_pass | 解析名 | 阶段 | 文件 |\n|---|---|---|---|---|\n")
        for i, (base, resolved, phase, path) in enumerate(rows):
            rel = os.path.relpath(path, d)
            f.write(f"| {i} | {base} | {resolved} | {phase} | [{os.path.basename(path)}]({rel}) |\n")

    # ---- intrinsic 演化:phase2 每步后的 nnp_* intrinsic 及参数个数 ----
    intrinsic_re = re.compile(r'"(nnp[-_][a-z0-9_]+)"')
    evo_lines = ["# nnp_* intrinsic 演化(phase2/3 逐 pass)\n",
                 "intrinsic 创建时只有几个参数,后续 pass 逐步追加 key(见 docs/02_middle.md §P2)。\n\n"]
    for base, resolved, phase, path in rows:
        if not phase or phase not in ("P2", "P3"):
            continue
        try:
            text = open(path, errors="ignore").read()
        except OSError:
            continue
        hits = intrinsic_re.findall(text)
        if not hits:
            continue
        from collections import Counter
        c = Counter(hits)
        args_m = re.findall(r'call_extern\("int32", "nnp[-_]([a-z0-9_]+)"(.*?)\)', text)
        arg_counts = {}
        for op, argstr in args_m:
            n = argstr.count(",")
            arg_counts.setdefault(op, set()).add(n)
        sig = ", ".join(f"{k}:{v}" for k, v in sorted(c.items()))
        argc = ", ".join(f"{k}({sorted(v)}参)" for k, v in sorted(arg_counts.items()))
        evo_lines.append(f"- **{base}** → {resolved} [{phase}]\n  - 种类×次数: {sig}\n  - 参数数: {argc}\n")
    with open(os.path.join(d, "intrinsic_evolution.md"), "w") as f:
        f.write("\n".join(evo_lines))

    # ---- DMA 视角 ----
    dma_ops = ["idma", "eidma", "odma", "eodma", "ewdma", "ebdma", "wbdma", "bdma"]
    dma_lines = ["# DMA 数据搬运演化\n\n| dump | pass | " + " | ".join(dma_ops) + " |\n",
                 "|" + "---|" * (len(dma_ops) + 2) + "\n"]
    for base, resolved, phase, path in rows:
        try:
            text = open(path, errors="ignore").read()
        except OSError:
            continue
        counts = [text.count(f"nnp_{o}") + text.count(f"nnp_{o}".replace("nnp_", "nnp-"))
                  for o in dma_ops]
        if sum(counts):
            dma_lines.append(f"| {base} | {resolved} | " + " | ".join(map(str, counts)) + " |\n")
    with open(os.path.join(d, "dma_view.md"), "w") as f:
        f.write("".join(dma_lines))

    print(f"  索引 {len(rows)} 个 dump 文件 → {d}/{{index.md, intrinsic_evolution.md, dma_view.md}}")
    print(f"  pass_list 解析:{len(pass_names)} 个自定义 pass 名")
    ctx.tir_index_dir = d
    return ctx


if __name__ == "__main__":
    run()
