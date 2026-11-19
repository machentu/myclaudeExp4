"""公共配置:目录布局、目标芯片、阶段产物路径。

每阶段 s<N>_<name>.py:
  - 暴露 run(ctx) -> ctx   (ctx 是跨阶段共享的 dict,内存对象直传)
  - __main__ 单独运行:自动向前重算缺失的上游阶段
产物落盘到 work/<chip>/s<N>_<name>/,便于逐步检查与修改实验。
"""
import os
import sys
import time

PIPE_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = os.environ.get("NPUP_TARGET", "nnp318p")   # 聚焦 nnp318p(PE_NUM=4,最能暴露 tile 切分)
INPUT_NAME = "data"
INPUT_SHAPE = (1, 3, 32, 32)                        # 32×32 在 nnp318p 上必然触发 tile 切分


def work_root():
    """work/<chip>/ 默认;设 NPUP_RUN_TAG=<模型名> 时为 work/<chip>/<tag>/(多模型隔离)。"""
    tag = os.environ.get("NPUP_RUN_TAG", "").strip().strip("/")
    return os.path.join(PIPE_ROOT, "work", TARGET, tag) if tag else \
           os.path.join(PIPE_ROOT, "work", TARGET)


def stage_dir(name):
    d = os.path.join(work_root(), name)
    os.makedirs(d, exist_ok=True)
    return d


class Ctx(dict):
    """跨阶段上下文;dict 语法 + 属性访问。"""
    def __getattr__(self, k):
        try:
            return self[k]
        except KeyError:
            raise AttributeError(k)

    def __setattr__(self, k, v):
        self[k] = v


def stage_banner(title):
    print("\n" + "=" * 78)
    print(f"[{time.strftime('%H:%M:%S')}] {title}")
    print("=" * 78, flush=True)


def ensure(ctx, stage_mod_name, key):
    """standalone 运行时,若 ctx 缺 key 则 import 上游阶段并执行。"""
    if key in ctx:
        return ctx[key]
    import importlib
    stage_banner(f"(自动补跑上游阶段 stages/{stage_mod_name}.py)")
    mod = importlib.import_module(f"stages.{stage_mod_name}")
    ctx = mod.run(ctx)
    return ctx[key]
