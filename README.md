# Terminal de pagamento com detecção de fraude — TinyML no ESP32-S3 (Wokwi)

**UC: IA Embarcada e Modelos Compactos** — Pós-graduação em IA Aplicada (UniSENAI) · Daniel Quiteque

Um terminal de pagamento simulado decide **localmente** se cada compra é fraude, sem enviar os dados do cartão para a nuvem (privacidade/LGPD, latência de milissegundos, funciona sem rede).

Pipeline do projeto final: **coleta de dados → treinamento → conversão e compressão → inferência no dispositivo**
(teclado + RTC → features no ESP32 → quantização int8 → MLP no TFLite Micro → APROVADA / BLOQUEADA).

| | |
|---|---|
| Dados | *Credit Card Transactions Fraud Detection* (Kaggle, gerado pelo simulador público Sparkov): 1,78 M transações, 0,53% de fraude |
| Sensores / entradas | Teclado matricial 4×4 (valor, categoria) + **RTC DS1307 via I2C** (hora da compra) |
| Features calculadas no ESP32 | Valor, hora (sin/cos), idade do titular, tempo desde a compra anterior, valor vs. média do cartão, nº e soma de compras nas últimas 24 h, categoria (22 no total) |
| Modelo embarcado | MLP 22-32-16-1 · **QAT int8 · 4.168 B · tensor arena 1,6 KB** |
| Qualidade (cartões nunca vistos) | PR-AUC 0,899 (igual ao float32) · recall 89% |
| Demo (replay no Wokwi) | **44 de 45 fraudes bloqueadas**, 1 falso alarme em 88 compras legítimas |
| Saídas | OLED SSD1306, LED verde/vermelho, buzzer, MQTT → dashboard |

## Documentação

| Documento | Conteúdo |
|---|---|
| [Apresentação (PDF)](docs/apresentacao_projeto1_antifraude.pdf) | Os 14 slides apresentados |
| Relatório técnico — [Markdown](docs/relatorio_tecnico.md) · [Word](docs/relatorio_tecnico.docx) · [PDF](docs/relatorio_tecnico.pdf) | Objetivo, dados, modelo, compressão, arquitetura, hardware, testes, resultados, problemas e soluções, evidências |
| [Roteiro da apresentação](docs/roteiro_apresentacao.md) | 14 slides: o que mostrar, o que falar, tempo e pontos técnicos |
| [Roteiro da demonstração](docs/roteiro_demo.md) | Os 4 testes ao vivo, com resultados esperados e verificados |
| [Perguntas da banca](docs/apresentacao.md) | Respostas prontas para a arguição |

---

## Estrutura

```
├── notebooks/
│   ├── 01_antifraude_treino_compressao.ipynb   ← coleta, features, treino, compressão, exportação
│   └── artefatos_antifraude/                   ← .tflite, tabela de compressão, replay esperado
├── firmware/                                   ← projeto ESP-IDF + diagram.json + wokwi.toml
│   ├── main/        main.cc · fraud_features.c · fraud_model.cc · keypad.c · ds1307.c · board.c
│   │                model_data.cc · fraud_params.h · replay_data.h   (gerados pelo notebook)
│   └── components/  ssd1306 (OLED I2C + fonte 5x7) · net (Wi-Fi Wokwi-GUEST + MQTT, opcional)
├── wokwi_web/       firmware.bin + diagram.json + wokwi.toml (Wokwi no VS Code ou no navegador, sem ESP-IDF)
├── dashboard/       painel Streamlit (MQTT ao vivo ou modo offline)
├── tests/           run_parity.sh — prova que o C do firmware = Python do notebook
├── tools/           desabilitar_esp_nn.py
└── docs/            apresentacao (PDF) · relatorio_tecnico.md · roteiro_apresentacao.md · roteiro_demo.md · apresentacao.md (perguntas) · evidencias/
```

## Como cada tópico da disciplina foi aplicado

| Tópico | Onde |
|---|---|
| Coleta de dados de sensores | Teclado matricial (GPIO) + RTC DS1307 (I2C) |
| Dataset público | Sparkov/Kaggle — o notebook gera os dados com o mesmo simulador (sem credencial) ou baixa do Kaggle |
| Contagem de parâmetros, estimativa Flash/RAM (Aula 3) | Seção 3.1 do notebook |
| Tensor arena **medida** | `tflm_arena()` roda o runtime TFLite Micro e lê a alocação real (+50% de folga) |
| float16, *dynamic range*, **PTQ int8** | Seção 4, com a conta `q = round(x/scale) + zero_point` |
| **QAT** | Seção 4.2 — modelo escolhido para embarcar |
| **Pruning** 75% · **Clustering** 16 | Seções 4.3 e 4.4 (efeito no tamanho bruto × gzip) |
| **Knowledge distillation** | Seção 4.5, com grupo de controle (aluno sem professor) |
| `.tflite` → array C (`xxd -i`) | Seção 6 — `model_data.cc` com `alignas(16)` |
| TFLM no ESP32-S3 via ESP-IDF (Aula 4) | `fraud_model.cc` (`MicroMutableOpResolver` só com FULLY_CONNECTED e LOGISTIC) |
| ESP-NN desligado para o Wokwi | `sdkconfig.defaults` + `tools/desabilitar_esp_nn.py` |
| MQTT para dashboard | `firmware/components/net` + `dashboard/` |

## Resultados — teste com cartões nunca vistos no treino

| modelo | .tflite | gzip | arena TFLM | PR-AUC | roda no TFLM? |
|---|---:|---:|---:|---:|:-:|
| float32 | 7.156 B | 5.797 B | 1.696 B | 0,899 | sim |
| float16 | 5.388 B | 3.694 B | – | 0,899 | **não** |
| PTQ int8 | 4.968 B | 2.845 B | 1.984 B | 0,894 | sim |
| **QAT int8 (embarcado)** | **4.168 B** | 2.398 B | **1.600 B** | **0,899** | sim |
| pruning 75% + int8 | 4.968 B | 2.133 B | 1.984 B | 0,739 | sim |
| clustering 16 + int8 | 4.968 B | 2.043 B | 1.984 B | 0,776 | sim |
| aluno destilado (16-8) | 3.536 B | 1.810 B | 1.776 B | 0,784 | sim |
| aluno sem professor (16-8) | 3.656 B | 1.843 B | 1.776 B | 0,807 | sim |

Leitura: a QAT recupera a perda da PTQ e gera o menor modelo completo. Pruning e clustering só reduzem o tamanho comprimido (gzip) e custam PR-AUC num modelo de ~1,3 mil parâmetros. A destilação não ajudou: o aluno sem professor foi melhor, porque com 1,2 M exemplos os rótulos bastam.

---

## Como rodar

### 1. Treino (Google Colab ou local) — opcional
Abra `notebooks/01_antifraude_treino_compressao.ipynb`, descomente o `pip install` da primeira célula e execute tudo. Ele gera os dados, treina, comprime e escreve os headers em `firmware/main/`. Os headers já gerados estão no repositório.

### 2a. Wokwi sem instalar o ESP-IDF (recomendado)
**VS Code + extensão Wokwi:** abra a pasta `wokwi_web/` (marque-a como confiável) → **F1 → Wokwi: Start Simulator**. O `wokwi.toml` aponta para o `firmware.bin` já compilado; o botão ↻ reinicia o firmware e zera o histórico do cartão.

**Navegador:** siga `wokwi_web/LEIA-ME.txt` (novo projeto ESP32-S3 → cole o `diagram.json` → **F1 → Upload Firmware and Start Simulation** → `firmware.bin`). Não use o botão verde do site: ele recompila o sketch de exemplo.

### 2b. Firmware no VS Code (ESP-IDF ≥ 5.2, testado com v5.4.2 + extensão Wokwi)
```bash
cd firmware
idf.py set-target esp32s3
idf.py build
python ../tools/desabilitar_esp_nn.py .   # só se o esp-tflite-micro baixado for 1.3.x; depois: idf.py build
```
`F1 → Wokwi: Start Simulator`. O monitor serial mostra uma linha `TX,...` por compra.

**Teclas:** `0–9` valor em centavos · `#` pagar · `*` apagar · `C` limpar · `A` próxima categoria · `B` adianta o relógio 1 h · `D` liga/desliga o replay de transações reais com rajadas de fraude.
Para ver uma fraude: siga os 4 testes de [docs/roteiro_demo.md](docs/roteiro_demo.md) — US$ 900 em compras web é aprovado às 14h e bloqueado às 2h.

### 3. Dashboard
```bash
cd dashboard && python -m pip install -r requirements.txt && python -m streamlit run app.py
```
**MQTT**: broker `broker.hivemq.com`, prefixo `ia-embarcada/daniel` (o mesmo do `idf.py menuconfig → Rede`), com o Wokwi rodando. **Offline**: reproduz as saídas do notebook, sem simulador.

### 4. Teste de paridade C × Python
```bash
bash tests/run_parity.sh     # esperado: [antifraude] saídas int8 idênticas: 133/133
```

## Decisões e limitações
* **Divisão por cartão** (clientes do teste nunca vistos). A divisão temporal caiu para PR-AUC 0,17 por *drift* do simulador entre 2019 e 2020. Em produção, isso exigiria monitoramento e re-treino.
* **Dados sintéticos:** dados reais de cartão por transação não são públicos (LGPD/PCI). O único dataset real público (ULB) traz variáveis anonimizadas por PCA, que nenhum terminal consegue calcular.
* **TFLite × TFLM:** os kernels de referência do TFLM arredondam um pouco diferente. Por isso, a simulação final do notebook usa o próprio runtime TFLM.
