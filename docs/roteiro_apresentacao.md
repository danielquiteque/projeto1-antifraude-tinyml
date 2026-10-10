# Roteiro da apresentação — 10 minutos + 5 de perguntas

Slides: https://claude.ai/artifact/Crcg8raprj5xGcxKPkyncU (exportar em PDF para a entrega) · demonstração: [roteiro_demo.md](roteiro_demo.md)
Perguntas prováveis da banca e respostas: [apresentacao.md](apresentacao.md)

**Preparação (antes de entrar):** VS Code com a aba *Wokwi Simulator* aberta (pasta `wokwi_web`), dashboard rodando
(`python -m streamlit run app.py`) em modo MQTT, os dois lado a lado. Clique em ↻ no Wokwi e em *Limpar dados* no painel.

| # | Slide | Tempo | O que aparece | O que falar | Pontos técnicos a destacar |
|---|---|---|---|---|---|
| 1 | Capa | 0:00–0:20 | Título, nome, UC, link do GitHub | "Um terminal de pagamento que detecta fraude sozinho, dentro de um microcontrolador, com uma rede neural de 4 KB." | — |
| 2 | Problema | 0:20–0:55 | 3 cartões (privacidade, sem rede, latência) + aplicações | Hoje a decisão é no servidor do banco: custa privacidade, depende de rede e tem latência. Sem rede, recusa ou aprova às cegas. | LGPD; aplicações: POS, transporte, vending, wearables |
| 3 | Objetivo | 0:55–1:15 | Tabela das 4 etapas do enunciado | O projeto cobre as 4 etapas pedidas, todas no mesmo dispositivo. | MQTT é só telemetria |
| 4 | Dados | 1:15–1:55 | 1,78 mi transações · 0,53 % fraude · 993 cartões + tabela legítima × fraude | Muitos dados, pouquíssima fraude. Fraude é de madrugada, em rajadas e acima do padrão do cliente. | Desbalanceamento → PR-AUC e recall, não acurácia; dataset Sparkov reprodutível |
| 5 | **Tratamento das variáveis** | 1:55–2:40 | Tabela variável bruta → tratamento → motivo | Só uso o que o terminal calcula. Log no valor, seno/cosseno na hora, média móvel do cliente, janela de 24 h, one-hot na categoria. | Hora circular; casos de borda (1ª compra: Δt = 7 dias, EMA = 0); padronização com μ/σ **só do treino** |
| 6 | Rede neural | 2:40–3:15 | Diagrama 22 → 32 → 16 → 1 + por que MLP, métrica, limiar | MLP pequeno, 1.281 parâmetros. Limiar escolhido para recall ≥ 90 %. | Conta à mão 736 + 528 + 17; só 2 operadores no TFLM; limiar 7,4 % = q ≥ −109 |
| 7 | **Treinamento** | 3:15–3:55 | Tabela treino/validação/teste + hiperparâmetros + curva PR-AUC por época | Divisão por cartão (testa clientes inéditos). Adam, 25 épocas, early stopping. Desbalanceamento tratado no limiar. | 620 passos/época; class_weight testado e descartado; validação 0,67 → 0,884 sem sobreajuste; divisão temporal caiu para 0,17 (drift) |
| 8 | Compressão | 3:55–4:45 | Tabela de 7 variantes, QAT destacado | QAT int8 ficou com o menor arquivo e o mesmo PR-AUC do float32. | fp16 não roda no TFLM; `q = round(x/scale) + zp`; pruning/clustering só ganham no gzip; KD não ajudou (controle) |
| 9 | Arquitetura e jornada de uso | 4:45–5:15 | Diagrama sensores → ESP32 → broker → dashboard + **5 passos da jornada no caixa** | A decisão é local; a nuvem só recebe o resultado. Jornada: digita → paga → decide → sinaliza → monitora. | Sem rede, os passos 1 a 4 continuam funcionando; notebook gera `model_data.cc` e `fraud_params.h` |
| 10 | Hardware e firmware | 5:15–5:45 | Tabela de componentes e pinos + **print do Wokwi com o LED verde aceso** | Dois sensores: teclado matricial e RTC DS1307. OLED e RTC dividem o I2C. Firmware em módulos no ESP-IDF. | I2C 0x3C / 0x68 nos GPIO 8/9; TFLM com 2 operadores e arena estática; ESP-NN desligado no Wokwi |
| 11 | Backend e interface | 5:45–6:15 | **Print do dashboard** + caixa "Como o dado viaja" (5 passos) | O ESP32 publica um JSON por compra (MQTT QoS 0); o broker só repassa; o painel guarda as últimas 2.000 decisões em memória e atualiza a cada 1 s. | Sem banco de dados por decisão de projeto; backend validado com cliente MQTT independente; µs medidos no chip |
| 12 | **Demonstração** | 6:15–8:30 | Tabela dos 4 testes (troque para o Wokwi + painel) | Teste 1 verde; teste 2 US$ 900 **em compras web** de dia verde; teste 3 mesma compra às 2h **vermelha**; teste 4 replay se sobrar tempo. | Tecla B = RTC +1 h; modelo usa contexto, não só valor; previsto q = 117, medido q = 116 |
| 13 | Resultados | 8:30–9:15 | **Matriz de confusão do ESP32** (87 VN · 1 FP · 1 FN · 44 VP) + 4 números | No chip, 44 de 45 fraudes bloqueadas e 1 falso alarme. No teste completo: recall 0,89, precisão 0,64. Depois: 4 KB, 812 B, ~2 ms, PR-AUC igual ao float32. | Recall = VP/(VP+FN); precisão = VP/(VP+FP); FP = 1ª compra sem histórico; FN = US$ 18 com P = 5,9 % |
| 14 | Conclusão | 9:15–10:00 | Entregue × limitações e próximos passos + "Perguntas?" | Um microcontrolador barato decide sozinho com a qualidade de um float32. Limitação: dados sintéticos e drift → re-treino e OTA. | — |

Saíram para caber em 10 min: o slide "Software e backend" (fundido nos slides 10 e 11) e o slide "Problemas" (a tabela completa está na seção 17 do relatório e serve para a arguição).

## Dicas de tempo
- O slide 12 (demonstração) é o mais longo: **2 min 15 s**. Se atrasar, pule o teste 4 (replay).
- Os slides 5, 7 e 8 (tratamento, treino e compressão) concentram as perguntas técnicas: fale devagar.
- Ensaie com cronômetro: a apresentação termina aos **10:00** e o tempo faz parte da nota.

## Perguntas prováveis sobre tratamento e treino
- **Por que log no valor?** A distribuição tem cauda longa; o log aproxima as escalas e a rede aprende melhor.
- **Por que seno e cosseno na hora?** Um número de 0 a 23 diria que 23h e 0h estão longe; no círculo elas são vizinhas.
- **Por que μ e σ só do treino?** Usar estatísticas da validação/teste vazaria informação e inflaria as métricas.
- **E o desbalanceamento?** `class_weight` foi testado e piorou o PR-AUC; o desbalanceamento é tratado no limiar (recall ≥ 90 %).
- **Qual a matriz de confusão no teste?** Modelo embarcado: ≈ 257.023 VN, 746 FP, 157 FN, 1.299 VP. Comparado ao float32 (862 FP, 152 FN), tem 116 falsos alarmes a menos.
- **Por que não acurácia?** Com 0,5 % de fraude, "sempre legítima" acerta 99,5 % e não pega nenhuma fraude.
- **Como a inferência funciona no chip?** Features em float → quantiza (`q = round(x/scale) + zp`) → 3 camadas FULLY_CONNECTED int8 (acumulação int32 + requantização, ReLU fundida) → LOGISTIC → compara `q ≥ −109`.
- **Onde os dados ficam guardados?** Em nenhum banco: o broker só repassa e o painel guarda as últimas 2.000 decisões em memória (somem ao reiniciar). É proposital: nenhum dado de cartão persistido fora do terminal.
- **Qual a acurácia?** 99,6 %, mas não é a métrica usada: "sempre legítima" teria 99,4 % sem pegar nenhuma fraude. Por isso PR-AUC (0,899) e recall (0,89).
- **Houve sobreajuste?** Não: treino (0,885) e validação (0,884) terminam juntos; o teste em clientes inéditos deu 0,899.

## Plano B
- Wokwi travou → mostre os slides 10 e 11 (prints com resultados reais) e o modo **Offline** do dashboard.
- Sem internet → a inferência continua local: mostre OLED, LEDs e o serial no VS Code.
