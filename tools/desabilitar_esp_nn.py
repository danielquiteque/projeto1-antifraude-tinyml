"""Desliga o ESP-NN no esp-tflite-micro baixado pelo Component Manager (necessário para simular no Wokwi).

Contexto (Aula 4): o Wokwi não simula as instruções SIMD que o ESP-NN usa. Na versão 1.3.x do
esp-tflite-micro a solução é comentar a linha `-DESP_NN` no CMakeLists.txt do componente.
A partir da 1.4 existe a opção Kconfig CONFIG_ESP_TFLITE_MICRO_USE_ESP_NN (já desligada no
sdkconfig.defaults deste repositório) e este script não precisa fazer nada.

Uso (depois do primeiro `idf.py build`, que baixa managed_components/):
    python tools/desabilitar_esp_nn.py firmware
"""
import re
import sys
from pathlib import Path


def main(fw_dir: str) -> int:
    cm = Path(fw_dir) / "managed_components" / "espressif__esp-tflite-micro" / "CMakeLists.txt"
    if not cm.exists():
        print(f"não encontrei {cm} — rode `idf.py build` uma vez para baixar os componentes")
        return 1
    txt = cm.read_text()
    if "CONFIG_ESP_TFLITE_MICRO_USE_ESP_NN" in txt:
        print("esp-tflite-micro >= 1.4: o ESP-NN é controlado pelo sdkconfig (já está desligado). Nada a fazer.")
        return 0
    pat = re.compile(r"^(\s*)(target_compile_options\(\$\{COMPONENT_LIB\}\s+PRIVATE\s+-DESP_NN\))", re.M)
    if not pat.search(txt):
        print("linha -DESP_NN já comentada (ou não encontrada). Nada a fazer.")
        return 0
    cm.write_text(pat.sub(r"\1# \2  # desligado para o Wokwi", txt))
    print(f"ESP-NN desligado em {cm}. Rode `idf.py fullclean build`.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "firmware"))
