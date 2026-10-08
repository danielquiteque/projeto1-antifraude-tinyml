"""Dashboard do terminal antifraude (TinyML no ESP32-S3).

Fonte dos dados:
  * MQTT  — o ESP32 (no Wokwi ou físico) publica cada transação em <prefixo>/antifraude/tx
  * Offline — reproduz as saídas do modelo int8 geradas pelo notebook (replay_esperado.csv), para testar sem o Wokwi

Rodar:  pip install -r requirements.txt && streamlit run app.py
"""
import json
import queue
import re
from collections import deque
from pathlib import Path

import pandas as pd
import plotly.graph_objects as go
import streamlit as st

ROOT = Path(__file__).resolve().parent.parent
FRAUD_CSV = ROOT / "notebooks/artefatos_antifraude/replay_esperado.csv"
FRAUD_PARAMS = ROOT / "firmware/main/fraud_params.h"

# Paleta do tema azul-escuro (mesmos tons do .streamlit/config.toml)
C_BG = "#0a1628"
C_CARD = "#11223d"
C_BORDER = "#1f3a63"
C_TEXT = "#e6edf7"
C_MUTED = "#8fa3bf"
C_SERIES = "#4da3ff"
C_GOOD = "#2ecc71"
C_CRIT = "#ff5c5c"
C_WARN = "#f5b041"

# Rótulos em português, na mesma ordem/chaves do firmware (kFraudCategoryKeys → kCatLabel)
CAT_LABEL = {
    "entertainment": "entretenimento", "food_dining": "restaurante", "gas_transport": "combustível",
    "grocery_net": "mercado web", "grocery_pos": "mercado loja", "health_fitness": "saúde/fitness",
    "home": "casa", "kids_pets": "crianças/pets", "misc_net": "diversos web", "misc_pos": "diversos loja",
    "personal_care": "cuidado pessoal", "shopping_net": "compras web", "shopping_pos": "compras loja",
    "travel": "viagem",
}

st.set_page_config(page_title="Antifraude TinyML", page_icon="💳", layout="wide")

CSS = f"""
<style>
  .block-container {{ padding-top: 2rem; }}
  .hero {{ background: linear-gradient(120deg, #0f2a52 0%, #133a73 60%, #1d4f99 100%);
           border: 1px solid {C_BORDER}; border-radius: 14px; padding: 18px 24px; margin-bottom: 14px; }}
  .hero h1 {{ color: {C_TEXT}; font-size: 1.7rem; margin: 0; padding: 0; }}
  .hero p  {{ color: #b9cbe6; margin: 4px 0 0 0; font-size: .95rem; }}
  .kpi {{ background: {C_CARD}; border: 1px solid {C_BORDER}; border-left: 4px solid var(--accent);
          border-radius: 12px; padding: 12px 16px; height: 100%; }}
  .kpi .lbl {{ color: {C_MUTED}; font-size: .78rem; text-transform: uppercase; letter-spacing: .06em; }}
  .kpi .val {{ color: {C_TEXT}; font-size: 1.7rem; font-weight: 700; margin-top: 2px; }}
  .last {{ background: {C_CARD}; border: 1px solid {C_BORDER}; border-radius: 14px; padding: 16px 20px;
           display: flex; align-items: center; gap: 22px; margin: 14px 0 6px 0; }}
  .badge {{ font-weight: 800; font-size: 1.25rem; padding: 10px 18px; border-radius: 10px; color: #06121f; }}
  .last .info {{ color: {C_TEXT}; font-size: 1rem; line-height: 1.55; }}
  .last .info span {{ color: {C_MUTED}; }}
  .pill {{ display: inline-block; padding: 3px 12px; border-radius: 999px; font-size: .8rem;
           background: {C_CARD}; border: 1px solid {C_BORDER}; color: {C_MUTED}; margin-bottom: 8px; }}
  h3 {{ color: {C_TEXT}; }}
</style>
"""


def read_threshold():
    """Lê o limiar de decisão do header gerado pelo notebook (o mesmo que o firmware usa)."""
    m = re.search(r"#define FRAUD_THRESHOLD\s+([\d.]+)f", FRAUD_PARAMS.read_text()) if FRAUD_PARAMS.exists() else None
    return float(m.group(1)) if m else None


def style(fig, h=300):
    fig.update_layout(
        height=h, margin=dict(l=10, r=10, t=30, b=10), plot_bgcolor=C_CARD, paper_bgcolor="rgba(0,0,0,0)",
        font=dict(family='system-ui, -apple-system, "Segoe UI", sans-serif', size=12, color=C_MUTED),
        hovermode="x unified", legend=dict(orientation="h", y=1.12, x=0, font=dict(color=C_TEXT)),
        hoverlabel=dict(bgcolor=C_BG, font_color=C_TEXT),
    )
    fig.update_xaxes(gridcolor=C_BORDER, linecolor=C_BORDER, zeroline=False)
    fig.update_yaxes(gridcolor=C_BORDER, linecolor=C_BORDER, zeroline=False)
    return fig


# --------------------------------------------------------------------------------------------
# Fonte MQTT (thread em segundo plano, compartilhada entre reruns do Streamlit)
class MqttFeed:
    def __init__(self, host, port, prefix):
        import paho.mqtt.client as mqtt

        self.host, self.port, self.prefix, self.q = host, port, prefix, queue.Queue()
        self.connected = False
        self.cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.cli.on_connect = self._on_connect
        self.cli.on_message = lambda c, u, m: self.q.put((m.topic, m.payload.decode(errors="ignore")))
        self.cli.connect_async(host, port, keepalive=30)
        self.cli.loop_start()

    def _on_connect(self, c, u, flags, rc, props=None):
        self.connected = True
        c.subscribe(f"{self.prefix}/#")

    def drain(self):
        out = []
        while not self.q.empty():
            out.append(self.q.get_nowait())
        return out


@st.cache_resource
def get_feed(host, port, prefix):
    return MqttFeed(host, port, prefix)


@st.cache_resource
def get_store():
    return {"tx": deque(maxlen=2000), "status": {}, "off_i": [0]}


# --------------------------------------------------------------------------------------------
# Blocos visuais
def kpi(col, label, value, accent):
    col.markdown(f'<div class="kpi" style="--accent:{accent}"><div class="lbl">{label}</div>'
                 f'<div class="val">{value}</div></div>', unsafe_allow_html=True)


def render_kpis(tx):
    lab = tx[tx.real >= 0] if "real" in tx else tx.iloc[0:0]
    tp = int(((lab.real == 1) & (lab.alerta == 1)).sum())
    fn = int(((lab.real == 1) & (lab.alerta == 0)).sum())
    fp = int(((lab.real == 0) & (lab.alerta == 1)).sum())
    lat = tx.get("t_inf_us")
    c = st.columns(5)
    kpi(c[0], "Transações", len(tx), C_SERIES)
    kpi(c[1], "Bloqueadas", int(tx.alerta.sum()), C_CRIT)
    kpi(c[2], "Fraudes detectadas", f"{tp}/{tp + fn}" if tp + fn else "–", C_GOOD)
    kpi(c[3], "Falsos alarmes", fp if len(lab) else "–", C_WARN)
    kpi(c[4], "Inferência (média)", f"{lat.dropna().mean():.0f} µs" if lat is not None and lat.notna().any() else "–",
        "#a78bfa")


def render_last(row):
    blocked = int(row.alerta) == 1
    color, text = (C_CRIT, "BLOQUEADA") if blocked else (C_GOOD, "APROVADA")
    lat = f" · inferência {row.t_inf_us:.0f} µs" if pd.notna(row.get("t_inf_us")) else ""
    st.markdown(
        f'<div class="last"><div class="badge" style="background:{color}">{text}</div>'
        f'<div class="info"><b>${row.amt:,.2f}</b> · {CAT_LABEL.get(row["cat"], row["cat"])} · {row.hora}<br>'
        f'<span>P(fraude) = {row.p * 100:.1f}%{lat}</span></div></div>', unsafe_allow_html=True)


def render_chart(t, thr):
    fig = go.Figure()
    fig.add_trace(go.Scatter(x=t.index, y=t.p, mode="lines+markers", name="P(fraude)",
                             line=dict(color=C_SERIES, width=2), marker=dict(size=6),
                             customdata=t[["amt", "cat_pt", "hora"]],
                             hovertemplate="$%{customdata[0]:.2f} · %{customdata[1]} · %{customdata[2]}<br>p=%{y:.3f}"))
    a = t[t.alerta == 1]
    fig.add_trace(go.Scatter(x=a.index, y=a.p, mode="markers", name="bloqueada",
                             marker=dict(color=C_CRIT, size=11, symbol="x", line=dict(width=1, color=C_BG))))
    if "real" in t:
        f = t[t.real == 1]
        fig.add_trace(go.Scatter(x=f.index, y=[1.04] * len(f), mode="markers", name="fraude real (rótulo)",
                                 marker=dict(color=C_WARN, size=8, symbol="triangle-down")))
    if thr is not None:
        fig.add_hline(y=thr, line=dict(color=C_CRIT, dash="dash", width=1),
                      annotation_text=f"limiar {thr * 100:.1f}%", annotation_font_color=C_MUTED)
    fig.update_yaxes(range=[0, 1.08], title="probabilidade de fraude")
    fig.update_xaxes(title="transação")
    st.plotly_chart(style(fig, 320), width="stretch")


def render_table(t):
    names = {"i": "#", "card": "cartão", "hora": "hora", "cat_pt": "categoria", "amt": "valor ($)",
             "p": "P(fraude)", "real": "rótulo real", "cnt24": "compras 24h", "t_inf_us": "inferência (µs)"}
    cols = [c for c in names if c in t]
    show = t[cols + ["alerta"]].iloc[::-1].head(15).dropna(axis=1, how="all").copy()
    show.insert(0, "decisão", show.pop("alerta").map({1: "🔴 BLOQUEADA", 0: "🟢 aprovada"}))
    if "real" in show:
        show["real"] = show.real.map({1: "fraude", 0: "legítima", -1: "–"})
    st.dataframe(show.rename(columns=names), hide_index=True, width="stretch",
                 column_config={"P(fraude)": st.column_config.ProgressColumn("P(fraude)", min_value=0, max_value=1,
                                                                             format="%.3f")})


# --------------------------------------------------------------------------------------------
st.markdown(CSS, unsafe_allow_html=True)
st.sidebar.title("💳 Antifraude TinyML")
src = st.sidebar.radio("Fonte", ["MQTT (ESP32 / Wokwi)", "Offline (saídas do notebook)"])
store = get_store()
if src.startswith("MQTT"):
    host = st.sidebar.text_input("Broker", "broker.hivemq.com")
    port = st.sidebar.number_input("Porta", value=1883)
    prefix = st.sidebar.text_input("Prefixo dos tópicos", "ia-embarcada/daniel")
    feed = get_feed(host, int(port), prefix)
else:
    speed = st.sidebar.slider("Velocidade (eventos/s)", 1, 20, 5)
if st.sidebar.button("Limpar dados"):
    store["tx"].clear(); store["off_i"][:] = [0]
st.sidebar.caption("MLP int8 (QAT) · 4.168 B · TFLite Micro · ESP32-S3")

st.markdown('<div class="hero"><h1>💳 Terminal Antifraude · TinyML</h1>'
            '<p>Cada compra é decidida <b>dentro do ESP32-S3</b> (teclado + RTC → 22 features → MLP int8 no TFLite Micro) '
            'e só o resultado chega aqui via MQTT.</p></div>', unsafe_allow_html=True)
THRESHOLD = read_threshold()


@st.fragment(run_every=1.0)
def live():
    # ---- ingestão ----
    if src.startswith("MQTT"):
        for topic, payload in feed.drain():
            try:
                d = json.loads(payload)
            except json.JSONDecodeError:
                continue
            if topic.endswith("antifraude/tx"):
                store["tx"].append(d)
            else:
                store["status"][topic] = d
        # st.sidebar não pode ser usado dentro de @st.fragment (Streamlit >= 1.37): status vai na área principal
        dot = "🟢 MQTT conectado" if feed.connected else "🟡 MQTT conectando…"
        st.markdown(f'<span class="pill">{dot} · {feed.host}:{feed.port} · {feed.prefix}/#</span>',
                    unsafe_allow_html=True)
    else:
        st.markdown('<span class="pill">📼 Modo offline · saídas int8 do notebook</span>', unsafe_allow_html=True)
        if FRAUD_CSV.exists():
            fr = pd.read_csv(FRAUD_CSV)
            for _ in range(speed):
                r = fr.iloc[store["off_i"][0] % len(fr)]; store["off_i"][0] += 1
                store["tx"].append(dict(src="offline", i=int(r.i), card=int(r.card), amt=float(r.amt), cat=r["cat"],
                                        hora=r.hora, p=float(r.p), alerta=int(r.alerta), real=int(r.real), t_inf_us=None))

    # ---- painel ----
    tx = pd.DataFrame(list(store["tx"]))
    if tx.empty:
        st.info("Aguardando transações… (no Wokwi: digite um valor e #, ou D para o replay)")
        return
    tx["cat_pt"] = tx["cat"].map(CAT_LABEL).fillna(tx["cat"])
    render_kpis(tx)
    render_last(tx.iloc[-1])
    t = tx.tail(200).reset_index(drop=True)
    render_chart(t, THRESHOLD)
    st.markdown("### Últimas decisões")
    render_table(t)


live()
