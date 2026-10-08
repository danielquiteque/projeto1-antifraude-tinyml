"""Dashboard do terminal antifraude (TinyML no ESP32-S3).

Fonte dos dados:
  * MQTT  — o ESP32 (no Wokwi ou físico) publica cada transação em <prefixo>/antifraude/tx
  * Offline — reproduz as saídas do modelo int8 geradas pelo notebook (replay_esperado.csv), para testar sem o Wokwi

Rodar:  pip install -r requirements.txt && streamlit run app.py
"""
import json
import queue
from collections import deque
from pathlib import Path

import pandas as pd
import plotly.graph_objects as go
import streamlit as st

ROOT = Path(__file__).resolve().parent.parent
FRAUD_CSV = ROOT / "notebooks/artefatos_antifraude/replay_esperado.csv"

# Paleta (referência de dataviz): série principal azul, status crítico vermelho, tinta neutra
C_SERIES = "#2a78d6"
C_SERIES_2 = "#1baf7a"
C_CRIT = "#d03b3b"
C_GOOD = "#0ca30c"
C_MUTED = "#898781"
C_GRID = "#e1e0d9"
C_SURF = "#fcfcfb"

st.set_page_config(page_title="Antifraude TinyML", page_icon="💳", layout="wide")


def style(fig, h=300):
    fig.update_layout(
        height=h, margin=dict(l=10, r=10, t=30, b=10), plot_bgcolor=C_SURF, paper_bgcolor="rgba(0,0,0,0)",
        font=dict(family='system-ui, -apple-system, "Segoe UI", sans-serif', size=12, color="#52514e"),
        hovermode="x unified", legend=dict(orientation="h", y=1.12, x=0),
    )
    fig.update_xaxes(gridcolor=C_GRID, linecolor="#c3c2b7", zeroline=False)
    fig.update_yaxes(gridcolor=C_GRID, linecolor="#c3c2b7", zeroline=False)
    return fig


# --------------------------------------------------------------------------------------------
# Fonte MQTT (thread em segundo plano, compartilhada entre reruns do Streamlit)
class MqttFeed:
    def __init__(self, host, port, prefix):
        import paho.mqtt.client as mqtt

        self.prefix, self.q = prefix, queue.Queue()
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
        st.sidebar.caption(("🟢 conectado" if feed.connected else "🟡 conectando…") + f" · {feed.prefix}/#")
    else:
        if FRAUD_CSV.exists():
            fr = pd.read_csv(FRAUD_CSV)
            for _ in range(speed):
                r = fr.iloc[store["off_i"][0] % len(fr)]; store["off_i"][0] += 1
                store["tx"].append(dict(src="offline", i=int(r.i), card=int(r.card), amt=float(r.amt), cat=r["cat"],
                                        hora=r.hora, p=float(r.p), alerta=int(r.alerta), real=int(r.real), t_inf_us=None))

    # ---- Antifraude ----
    tx = pd.DataFrame(list(store["tx"]))
    if tx.empty:
        st.info("Aguardando transações… (no Wokwi: digite um valor e #, ou D para o replay)")
    else:
        lab = tx[tx.real >= 0] if "real" in tx else tx.iloc[0:0]
        tp = int(((lab.real == 1) & (lab.alerta == 1)).sum()); fn = int(((lab.real == 1) & (lab.alerta == 0)).sum())
        fp = int(((lab.real == 0) & (lab.alerta == 1)).sum())
        c = st.columns(5)
        c[0].metric("Transações", len(tx))
        c[1].metric("Bloqueadas", int(tx.alerta.sum()))
        c[2].metric("Fraudes detectadas", f"{tp}/{tp + fn}" if tp + fn else "–")
        c[3].metric("Falsos alarmes", fp if len(lab) else "–")
        lat = tx.get("t_inf_us")
        c[4].metric("Inferência (média)", f"{lat.dropna().mean():.0f} µs" if lat is not None and lat.notna().any() else "–")

        t = tx.tail(200).reset_index(drop=True)
        fig = go.Figure()
        fig.add_trace(go.Scatter(x=t.index, y=t.p, mode="lines+markers", name="P(fraude)",
                                 line=dict(color=C_SERIES, width=2), marker=dict(size=6),
                                 customdata=t[["amt", "cat", "hora"]],
                                 hovertemplate="$%{customdata[0]:.2f} · %{customdata[1]} · %{customdata[2]}<br>p=%{y:.3f}"))
        a = t[t.alerta == 1]
        fig.add_trace(go.Scatter(x=a.index, y=a.p, mode="markers", name="bloqueada",
                                 marker=dict(color=C_CRIT, size=10, symbol="x", line=dict(width=2, color=C_SURF))))
        if "real" in t:
            f = t[t.real == 1]
            fig.add_trace(go.Scatter(x=f.index, y=[1.04] * len(f), mode="markers", name="fraude real (rótulo)",
                                     marker=dict(color=C_MUTED, size=8, symbol="triangle-down")))
        fig.update_yaxes(range=[0, 1.08], title="probabilidade")
        fig.update_xaxes(title="transação")
        st.plotly_chart(style(fig, 320), use_container_width=True)

        cols = [c for c in ["i", "card", "hora", "cat", "amt", "p", "alerta", "real", "cnt24", "t_inf_us"] if c in t]
        show = t[cols].iloc[::-1].head(15).dropna(axis=1, how="all").copy()
        show["decisão"] = show.alerta.map({1: "🔴 BLOQUEADA", 0: "🟢 aprovada"})
        st.dataframe(show, hide_index=True, use_container_width=True)



live()
