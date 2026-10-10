# Roteiro da demonstração ao vivo — 4 testes

Ambiente: **VS Code + extensão Wokwi** (pasta `wokwi_web/`, `F1 → Wokwi: Start Simulator`) e **dashboard** aberto
(`cd dashboard && python -m streamlit run app.py`, fonte **MQTT**). Tempo total: ~3 min.

## Antes de começar (fora do tempo da apresentação)
1. Clique em **↻ (restart)** na aba *Wokwi Simulator* — reiniciar **zera o histórico do cartão** de demonstração,
   e os resultados abaixo dependem disso.
2. Espere o terminal mostrar `MQTT conectado` e o dashboard mostrar **🟢 MQTT conectado**. Clique em **Limpar dados** no dashboard.
3. **Não faça compras de teste antes** — cada compra altera o histórico do cartão.
4. `B` = +1 hora no RTC por clique (um clique por vez, olhando a hora no OLED). `A` = próxima categoria.

## Os 4 testes

| # | Objetivo | Teclas | Esperado | Verificado em 08/10/2026 |
|---|---|---|---|---|
| 1 | Compra comum | `4` `5` `0` `0` `#` | 🟢 APROVADA, P ≈ 0–1 % | ✅ aprovada |
| 2 | Valor alto **de dia** | `A` até **compras web** → `9` `0` `0` `0` `0` `#` | 🟢 APROVADA, P ≈ 0 % | ✅ $900 · compras web · 14:14 → q = −127, **P = 0,4 %** |
| 3 | Mesma compra **de madrugada** | `B` até **02:xx** → `9` `0` `0` `0` `0` `#` | 🔴 BLOQUEADA, P ≈ 95 % + bipe duplo | ✅ $900 · compras web · 02:21 → q = **116**, **P = 95,3 %** |
| 4 | Replay de dados reais | `D` (aguarde ~45 s, até `tx 30/133`) | Rajada de 15 fraudes bloqueadas em sequência | ✅ dashboard: **15/15 fraudes**, 1 falso alarme |

Limiar de decisão: P ≥ 7,4 % (`q ≥ −109` na saída int8). Previsão do simulador para o teste 3: q = 117 → medido q = 116.

## O que falar em cada teste
1. **Compra comum** — "O ESP32 leu o teclado e o RTC, calculou 22 features com o histórico do cartão, quantizou para int8
   e rodou a rede no TFLite Micro em ~2,2 ms. Nada saiu do chip para decidir."
2. **Valor alto de dia** — "US$ 900 numa loja online, às 14h, é plausível. O modelo **não** bloqueia só pelo valor —
   uma regra fixa 'acima de X' daria falso alarme aqui."
3. **Madrugada** — "A tecla B simula a passagem do tempo no relógio de hardware. Mesma compra, mesma loja, só mudou a hora:
   95 % de fraude. O modelo aprendeu que fraudes se concentram entre 22h e 3h, em sequência e acima da média do cartão."
   → mostrar o cartão vermelho e o ✖ acima da linha do limiar no dashboard.
4. **Replay** — "São 133 transações reais de 3 cartões do conjunto de teste, nunca vistos no treino.
   No ciclo completo o terminal pega 44 das 45 fraudes com 1 falso alarme."

## Se algo der errado
- OLED apagado / serial com `Hello, ESP32-S3!` → está rodando o exemplo, não o firmware: use o VS Code (pasta `wokwi_web`).
- Teste 2 bloqueado → a categoria ficou em *mercado loja*: US$ 900 em mercado loja é bloqueado mesmo de dia
  (teste real: 19:47 → P = 78,5 %). Aperte `A` até o OLED mostrar **compras web** antes de digitar o valor.
- Teste 3 aprovado → a hora não está entre 00h e 03h, a categoria não é *compras web* ou o cartão tinha histórico:
  clique em ↻ e recomece do teste 1.
- Dashboard em 🟡 → a inferência continua local (OLED + LEDs + serial); mostre o modo **Offline** do dashboard.
