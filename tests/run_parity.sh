#!/usr/bin/env bash
# Prova que o código C do firmware produz as mesmas decisões que o notebook (replay_esperado.csv).
# Requer: gcc e python com numpy, pandas e tflite-micro.   Uso: bash tests/run_parity.sh
set -euo pipefail
cd "$(dirname "$0")"; M=../firmware/main; B=$(mktemp -d)
gcc -O2 -Wall -Wextra -I$M host_fraud_features.c $M/fraud_features.c -lm -o $B/t_fraud
$B/t_fraud > $B/fraud_c.csv
python compare.py $B/fraud_c.csv
