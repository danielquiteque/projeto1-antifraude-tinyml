"""Roda as features calculadas pelo código C no runtime TFLite Micro e compara com o notebook."""
import sys
from pathlib import Path

import numpy as np
import pandas as pd
from tflite_micro.python.tflite_micro import runtime

A = Path(__file__).resolve().parent.parent / "notebooks/artefatos_antifraude"


def lround_q(x, scale, zp):  # igual a lroundf() + saturação do firmware
    v = x / np.float32(scale)
    return np.clip(np.sign(v) * np.floor(np.abs(v) + .5) + zp, -128, 127).astype(np.int8)


it = runtime.Interpreter.from_bytes((A / "fraude_model.tflite").read_bytes(), arena_size=32 * 1024)
qp = it.get_input_details(0)["quantization_parameters"]
x = pd.read_csv(sys.argv[1], header=None).values[:, 1:].astype(np.float32)
y = []
for row in x:
    it.set_input(lround_q(row, qp["scales"][0], qp["zero_points"][0])[None], 0); it.invoke()
    y.append(int(it.get_output(0)[0, 0]))
e = pd.read_csv(A / "replay_esperado.csv")
ok = int((np.array(y) == e.y_q.values).sum())
print(f"[antifraude] saídas int8 idênticas: {ok}/{len(e)}")
sys.exit(0 if ok == len(e) else 1)
