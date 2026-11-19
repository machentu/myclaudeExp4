"""manifest — 每次 pipeline 运行的结构化产物清单(借鉴 nnp3xx-forge 的 CompiledArtifact 思想)。

每个阶段运行后记录:
  - 组件(厂商函数)/阶段名、墙钟耗时
  - 产物文件 + SHA256(★两次运行 hash 不同 = 不确定性暴露,如 VU 纳秒时间戳)
  - 关键指标(cycles/cosine/hex_len 等,由各阶段自行上报 ctx.metrics)

work/<chip>/<tag>/manifest.json;diff 两次 manifest 即可精确定位不可复现点。
"""
import hashlib
import json
import os
import time

from stages.common import work_root

_METRICS_KEYS = ("cycles", "cosine", "model_hex_len", "firmware_hex_len",
                 "mid_buf", "net_bin_size", "ddr_size", "weight_size")


def _hash_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()[:16]


class Manifest:
    def __init__(self, path):
        self.path = path
        self.data = {"stages": []}
        if os.path.isfile(path):
            try:
                self.data = json.load(open(path))
            except json.JSONDecodeError:
                pass

    def record(self, stage, desc, elapsed, artifact_globs, metrics):
        entry = {"stage": stage, "desc": desc, "elapsed_s": round(elapsed, 2),
                 "artifacts": {}}
        for g in artifact_globs:
            d = os.path.dirname(g)
            name = os.path.basename(g)
            if not os.path.isdir(d):
                continue
            for f in sorted(os.listdir(d)):
                if f.startswith(name) or f == name:
                    p = os.path.join(d, f)
                    if os.path.isfile(p) and os.path.getsize(p) < 64 << 20:
                        entry["artifacts"][f] = {
                            "bytes": os.path.getsize(p), "sha256_16": _hash_file(p)}
        picked = {k: metrics[k] for k in _METRICS_KEYS if k in metrics}
        if picked:
            entry["metrics"] = picked
        self.data["stages"].append(entry)
        self.flush()

    def flush(self):
        with open(self.path, "w") as f:
            json.dump(self.data, f, ensure_ascii=False, indent=2)
