# Roteiro da apresentação (10 min) e preparação para a arguição

> A apresentação tem até 10 min, seguidos de 5 min de perguntas individuais.

## Roteiro (≈ 10 min)

| tempo | slide / ação | mensagem-chave |
|---|---|---|
| 0:00–0:45 | Problema | Decidir **no terminal**: privacidade (LGPD), latência de ms e funcionamento sem rede |
| 0:45–1:45 | Dados | Sparkov/Kaggle, 1,78 M transações, 0,53% fraude. EDA: fraude de madrugada, em rajadas, com valor acima do padrão |
| 1:45–3:00 | Features **no dispositivo** | Só o que o ESP32 sabe: teclado + RTC + estado do cartão (Δt, EMA, janela 24 h). Nada de feature de backend |
| 3:00–4:00 | Modelo e memória | MLP 22-32-16-1 = 1.281 parâmetros (conta à mão), ~5 KB float32 e ~1,3 KB de pesos int8. Arena **medida** no TFLM |
| 4:00–5:30 | **Tabela de compressão** | fp16/dynamic não rodam no TFLM; PTQ −0,005 PR-AUC; **QAT recupera**; pruning/clustering só ganham no gzip; KD não ajudou (controle) |
| 5:30–7:30 | **Demo no Wokwi** | Compra normal → APROVADA; `B` até 2h, `compras web`, $900 três vezes → BLOQUEADA + buzzer; depois `D` (replay) |
| 7:30–9:00 | Dashboard MQTT | As mesmas decisões chegando ao painel; latência de inferência em µs |
| 9:00–10:00 | Limitações e próximos passos | Drift (divisão temporal), dados sintéticos, threshold adaptativo, OTA de modelo (Greengrass, Aula 8) |

**Dica de demo:** deixe o Wokwi já compilado e aberto. Se o Wi-Fi do simulador demorar, a inferência funciona offline (mostre o OLED e o monitor serial).

---

## Perguntas prováveis — e como responder

### Memória e hardware
**Quantos parâmetros tem o modelo e quanto ocupa?**
Camada Dense = entradas × neurônios + neurônios. 22·32+32 = 736; 32·16+16 = 528; 16·1+1 = 17 → **1.281** (bate com o `summary()` da seção 3.1). Em int8, 1 byte por peso (o bias fica em int32). O `.tflite` final tem 4.168 B: o resto é a estrutura do FlatBuffer e os parâmetros de quantização.

**Flash × RAM: o que vai em cada uma?**
Flash: os pesos (o array `g_model_data` é `const`). SRAM: a **tensor arena**, com as ativações intermediárias, os tensores de entrada/saída e as estruturas do interpretador. A arena foi **medida** com o runtime TFLM em Python (1.600 B) e reservada com 50% de folga (3 KB). O ESP32-S3 tem 512 KB de SRAM, então sobra muito.

**Por que não usar malloc?**
O TFLM foi feito para rodar sem alocação dinâmica: a arena é um array estático, alinhado em 16 bytes. Isso evita fragmentação e torna o uso de memória previsível.

### Quantização
**Explique `scale` e `zero_point`.**
Quantização afim: `q = round(x/scale) + zero_point`, saturado em [−128, 127]; o inverso é `x ≈ (q − zp)·scale`. O erro de arredondamento é ≤ scale/2. O firmware faz exatamente isso antes do `Invoke()` (`fraud_model.cc`).

**De onde vêm os ranges da quantização?**
Do *representative dataset* da PTQ: o conversor observa os valores reais de ativação. Incluí exemplos de fraude (a cauda da distribuição).

**Por que não usar o modelo float16, que é menor?**
O TFLite Micro não executa fp16 nem modelos híbridos (*dynamic range*). Em microcontrolador, a quantização tem de ser **int8 completa**.

**PTQ × QAT?**
A PTQ quantiza depois do treino e perdeu 0,005 de PR-AUC. A QAT insere *fake quant* no treino e o modelo aprende a compensar: recuperou o PR-AUC do float32 e ainda gerou um arquivo menor.

**Por que o threshold muda entre os modelos?**
A quantização desloca um pouco as probabilidades. Por isso recalibro o threshold na validação **com o modelo embarcado** e comparo direto no domínio int8 (`q >= FRAUD_THRESHOLD_Q`).

### Pruning, clustering, destilação
**O pruning de 75% não diminuiu o arquivo. Por quê?**
Os zeros continuam armazenados como int8 no formato denso. O ganho aparece no **gzip** (transmissão OTA) ou com kernels esparsos. Com ~1,3 mil parâmetros não há redundância sobrando: o PR-AUC caiu.

**E o clustering?**
Cada camada ficou com 16 valores distintos (veja a saída do notebook). Isso também só ajuda na compressão por gzip, e custou PR-AUC.

**A destilação piorou? Então por que está no trabalho?**
Porque é um resultado honesto, com grupo de controle. Com 1,2 milhão de exemplos, o aluno aprende bem só com os rótulos. A KD costuma ajudar quando há poucos dados ou quando o professor é muito superior.

### Dados e avaliação
**Por que PR-AUC e não acurácia?**
Com 0,5% de fraude, um modelo que diz sempre "legítima" tem 99,5% de acurácia. PR-AUC e recall medem o que importa: o falso negativo é o erro caro.

**Por que dividir por cartão e não por tempo?**
Testei a divisão temporal: o PR-AUC caiu para 0,17 porque o simulador tem *drift* (o valor mediano legítimo cai de US$ 52 para US$ 33). A divisão por cartão avalia clientes nunca vistos. Em produção, o *drift* é o que se monitora para disparar re-treino.

**Os dados são reais?**
São sintéticos, do simulador público Sparkov (o mesmo do dataset do Kaggle), porque dados reais de cartão são protegidos.

**Como você garante que o ESP32 calcula as mesmas features do Python?**
Com o `tests/run_parity.sh`: ele compila o **mesmo** `fraud_features.c` no PC e roda no runtime TFLM. Resultado: 133/133 saídas int8 idênticas.

### Embarcado / ESP-IDF
**Por que desligar o ESP-NN?**
O Wokwi não simula as instruções SIMD do ESP32-S3 que o ESP-NN usa. Sem ele, o TFLM cai nos kernels de referência: a inferência fica mais lenta, mas continua correta. Em hardware real dá para religar e medir o ganho.

**Quais operadores o modelo usa?**
`FULLY_CONNECTED` (ReLU fundida) e `LOGISTIC`. O `MicroMutableOpResolver` registra apenas esses, o que economiza Flash.

**E se a rede cair?**
A decisão é 100% local. O MQTT é só telemetria: o `net_start()` tem timeout e o firmware segue offline.
