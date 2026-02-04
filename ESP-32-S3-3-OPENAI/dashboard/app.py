import streamlit as st
import requests
import time
import os
import base64
from datetime import datetime
from collections import deque
from dotenv import load_dotenv, set_key
from gtts import gTTS

# Load environment variables
load_dotenv()
env_path = os.path.join(os.path.dirname(__file__), '.env')

# --- CONFIG ---
st.set_page_config(
    page_title="ESP32-S3 AI Smart Hub | Circuit Digest",
    page_icon="🤖",
    layout="wide",
    initial_sidebar_state="expanded"
)

# --- SESSION STATE ---
if "esp32_ip" not in st.session_state:
    st.session_state.esp32_ip = os.getenv("ESP32_IP", "192.168.32.2")
if "log_messages" not in st.session_state:
    st.session_state.log_messages = deque(maxlen=100)
if "last_sensors" not in st.session_state:
    st.session_state.last_sensors = {}
if "gemini_key" not in st.session_state:
    st.session_state.gemini_key = os.getenv("GEMINI_API_KEY", "")
if "chat_history" not in st.session_state:
    st.session_state.chat_history = []
if "autoplay" not in st.session_state:
    st.session_state.autoplay = True

API_BASE = f"http://{st.session_state.esp32_ip}"

# --- LOG SYSTEM ---
def log(msg, level="INFO"):
    timestamp = datetime.now().strftime('%H:%M:%S')
    st.session_state.log_messages.append(f"[{timestamp}] [{level}] {msg}")

# --- GEMINI DIRECT QUERY ---
def query_gemini(prompt, api_key):
    """Direct Gemini API query from Streamlit"""
    try:
        import google.generativeai as genai
        genai.configure(api_key=api_key)
        model = genai.GenerativeModel('gemini-3-flash-preview')
        response = model.generate_content(prompt)
        return response.text
    except ImportError:
        log("google-generativeai not installed. Run: pip install google-generativeai", "ERROR")
        return "Error: Gemini SDK not installed"
    except Exception as e:
        log(f"Gemini error: {e}", "ERROR")
        return f"Error: {e}"

# --- TEXT TO SPEECH ---
def text_to_speech(text):
    """Convert text to speech and return base64 audio"""
    try:
        tts = gTTS(text=text, lang='en')
        tts.save("response.mp3")
        with open("response.mp3", "rb") as f:
            audio_bytes = f.read()
            bin_str = base64.b64encode(audio_bytes).decode()
        return bin_str
    except Exception as e:
        log(f"TTS Error: {e}", "ERROR")
        return None

def autoplay_audio(text):
    """Generate audio and create an autoplay HTML element"""
    bin_str = text_to_speech(text)
    if bin_str:
        html = f'<audio autoplay="true" src="data:audio/mp3;base64,{bin_str}"></audio>'
        st.markdown(html, unsafe_allow_html=True)

# --- PREMIUM CSS ---
st.markdown("""
<style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');
    
    .stApp {
        background: linear-gradient(135deg, #0f0c29 0%, #302b63 50%, #24243e 100%);
        font-family: 'Inter', sans-serif;
    }
    
    .hero-container {
        background: linear-gradient(135deg, rgba(233, 69, 96, 0.1) 0%, rgba(15, 52, 96, 0.2) 100%);
        border: 1px solid rgba(233, 69, 96, 0.3);
        border-radius: 24px;
        padding: 40px;
        text-align: center;
        margin: 20px 0;
        backdrop-filter: blur(10px);
    }
    
    .hero-icon { font-size: 5rem; margin-bottom: 10px; }
    .hero-title {
        font-size: 2.5rem;
        font-weight: 700;
        background: linear-gradient(90deg, #e94560, #00ff88);
        -webkit-background-clip: text;
        -webkit-text-fill-color: transparent;
    }
    .hero-subtitle { color: #a0a0a0; font-size: 1.2rem; margin-top: 10px; }
    
    .metric-card {
        background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
        border: 1px solid #e94560;
        border-radius: 16px;
        padding: 24px;
        text-align: center;
        transition: all 0.3s ease;
        box-shadow: 0 4px 20px rgba(233, 69, 96, 0.15);
    }
    .metric-value { font-size: 2.2rem; font-weight: 700; color: #00ff88; }
    .metric-label { font-size: 0.9rem; color: #888; margin-top: 8px; text-transform: uppercase; }
    
    .log-container {
        background: #0a0a15;
        border: 1px solid #333;
        border-radius: 12px;
        padding: 15px;
        font-family: 'Consolas', monospace;
        font-size: 0.8rem;
        max-height: 300px;
        overflow-y: auto;
    }
    .log-info { color: #00ff88; }
    .log-error { color: #ff4444; }
    .log-warn { color: #ffaa00; }
    
    .chat-user { background: #1a1a2e; border-radius: 12px; padding: 12px; margin: 8px 0; }
    .chat-ai { background: #0f3460; border-radius: 12px; padding: 12px; margin: 8px 0; }
    
    .badge {
        display: inline-block;
        padding: 6px 16px;
        border-radius: 20px;
        font-size: 0.85rem;
        font-weight: 600;
    }
    .badge-online { background: rgba(0, 255, 136, 0.2); color: #00ff88; border: 1px solid #00ff88; }
    .badge-offline { background: rgba(255, 68, 68, 0.2); color: #ff4444; border: 1px solid #ff4444; }
</style>
""", unsafe_allow_html=True)

# --- HELPERS ---
def fetch(endpoint, default=None):
    try:
        log(f"GET /{endpoint}")
        r = requests.get(f"{API_BASE}/{endpoint}", timeout=3)
        if r.status_code == 200:
            data = r.json()
            log(f"Response: {data}")
            return data
        else:
            log(f"HTTP {r.status_code}", "ERROR")
            return default
    except requests.exceptions.ConnectionError:
        log(f"Connection refused to {API_BASE}", "ERROR")
        return default
    except Exception as e:
        log(f"Error: {e}", "ERROR")
        return default

def post(endpoint, data=None):
    try:
        log(f"POST /{endpoint} -> {data}")
        r = requests.post(f"{API_BASE}/{endpoint}", json=data, timeout=5)
        if r.status_code == 200:
            log(f"Success: {r.text}")
            return True
        else:
            log(f"HTTP {r.status_code}", "ERROR")
            return False
    except Exception as e:
        log(f"Error: {e}", "ERROR")
        return False

# --- SIDEBAR ---
with st.sidebar:
    st.markdown("### 📡 Device Connection")
    
    # Reload from .env button
    if st.button("📁 Load from .env", use_container_width=True):
        load_dotenv(override=True)
        st.session_state.esp32_ip = os.getenv("ESP32_IP", st.session_state.esp32_ip)
        st.session_state.gemini_key = os.getenv("GEMINI_API_KEY", st.session_state.gemini_key)
        st.rerun()

    new_ip = st.text_input("ESP32 IP Address", st.session_state.esp32_ip)
    if new_ip != st.session_state.esp32_ip:
        st.session_state.esp32_ip = new_ip
        set_key(env_path, "ESP32_IP", new_ip)
        st.toast("📡 IP saved to .env")
    
    if st.button("🔄 Test Connection", use_container_width=True):
        log(f"Testing connection to {st.session_state.esp32_ip}...")
        result = fetch("status")
        if result:
            st.success("✅ Connected!")
        else:
            st.error(f"❌ Cannot connect to {st.session_state.esp32_ip}")
    
    online = fetch("status") is not None
    if online:
        st.markdown('<span class="badge badge-online">🟢 Connected</span>', unsafe_allow_html=True)
    else:
        st.markdown('<span class="badge badge-offline">🔴 Offline</span>', unsafe_allow_html=True)
    
    st.markdown("---")
    st.markdown("### 🔑 Gemini API Key")
    new_gemini_key = st.text_input("API Key (saved to .env)", type="password", value=st.session_state.gemini_key)
    if new_gemini_key != st.session_state.gemini_key:
        st.session_state.gemini_key = new_gemini_key
        set_key(env_path, "GEMINI_API_KEY", new_gemini_key)
        st.toast("🔑 API Key saved to .env")

    st.markdown("---")
    st.markdown("### 📶 WiFi Setup")
    wifi_ssid = st.text_input("SSID")
    wifi_pass = st.text_input("Password", type="password")
    if st.button("💾 Save WiFi to ESP32", use_container_width=True):
        if post("settings/wifi", {"ssid": wifi_ssid, "password": wifi_pass}):
            st.success("✅ WiFi saved!")

# --- HERO SECTION ---
st.markdown("""
<div class="hero-container">
    <div class="hero-icon">🤖</div>
    <div class="hero-title">ESP32-S3 AI Smart Hub</div>
    <div class="hero-subtitle">Voice-Activated Home Assistant • Powered by Gemini AI</div>
    <div style="margin-top: 20px;">
        <span class="badge badge-online" style="margin: 5px;">DigiKey Challenge 2026</span>
        <span class="badge" style="background: rgba(233,69,96,0.2); color: #e94560; border: 1px solid #e94560; margin: 5px;">Circuit Digest</span>
    </div>
</div>
""", unsafe_allow_html=True)

# --- FETCH REAL SENSOR DATA ---
sensors = fetch("sensors", None)
if sensors is None:
    sensors = {"temp": "--", "hum": "--", "lux": "--", "presence": False}
else:
    st.session_state.last_sensors = sensors

col1, col2, col3, col4 = st.columns(4)

with col1:
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-value">🌡️ {sensors.get("temp", "--")}°C</div>
        <div class="metric-label">Temperature</div>
    </div>
    ''', unsafe_allow_html=True)

with col2:
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-value">💧 {sensors.get("hum", "--")}%</div>
        <div class="metric-label">Humidity</div>
    </div>
    ''', unsafe_allow_html=True)

with col3:
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-value">☀️ {sensors.get("lux", "--")}</div>
        <div class="metric-label">Ambient Light</div>
    </div>
    ''', unsafe_allow_html=True)

with col4:
    presence = "👤 Present" if sensors.get("presence") else "🚫 Away"
    st.markdown(f'''
    <div class="metric-card">
        <div class="metric-value">{presence}</div>
        <div class="metric-label">Presence</div>
    </div>
    ''', unsafe_allow_html=True)

st.markdown("<br>", unsafe_allow_html=True)

# === MAIN TABS ===
tab1, tab2, tab3, tab4, tab5 = st.tabs(["🤖 AI Chat", "🏠 Smart Controls", "⚙️ Settings", "📊 Analytics", "📋 Log Monitor"])

# --- AI CHAT (Direct Gemini) ---
with tab1:
    st.markdown("### 🤖 Chat with Gemini AI")
    st.markdown("Direct queries to Gemini from your browser - no ESP32 required!")
    
    if not st.session_state.gemini_key:
        st.warning("⚠️ Enter your Gemini API Key in the sidebar to start chatting")
    
    st.session_state.autoplay = st.toggle("🔊 Text-to-Speech (Auto-play)", value=st.session_state.autoplay)
    
    # Chat input
    user_input = st.text_area("Type your question:", height=100, key="chat_input")
    
    col1, col2 = st.columns([1, 4])
    with col1:
        if st.button("📤 Send", use_container_width=True, type="primary", disabled=not user_input or not st.session_state.gemini_key):
            log(f"Gemini query: {user_input}")
            with st.spinner("Thinking..."):
                response = query_gemini(user_input, st.session_state.gemini_key)
                st.session_state.chat_history.append({"user": user_input, "ai": response})
                log(f"Gemini response: {response[:100]}...")
                if st.session_state.autoplay:
                    autoplay_audio(response)
    with col2:
        if st.button("🗑️ Clear Chat", use_container_width=True):
            st.session_state.chat_history = []
            st.rerun()
    
    # Display chat history
    st.markdown("---")
    for chat in reversed(st.session_state.chat_history[-10:]):
        st.markdown(f'<div class="chat-user">👤 **You:** {chat["user"]}</div>', unsafe_allow_html=True)
        st.markdown(f'<div class="chat-ai">🤖 **AI:** {chat["ai"]}</div>', unsafe_allow_html=True)

# --- SMART CONTROLS ---
with tab2:
    st.markdown("### 🏠 Smart Home Controls")
    
    col1, col2, col3, col4 = st.columns(4)
    
    with col1:
        if st.button("💡 Lights", use_container_width=True):
            log("Toggling lights")
            post("action/lights")
            st.toast("Lights toggled!")
    
    with col2:
        if st.button("❄️ AC / Fan", use_container_width=True):
            log("Toggling AC")
            post("action/ac")
            st.toast("AC toggled!")
    
    with col3:
        if st.button("🛏️ Bed Mode", use_container_width=True):
            log("Activating bed mode")
            post("action/bed")
    
    with col4:
        if st.button("🔔 Blanket", use_container_width=True):
            log("Blanket control")
            post("action/blanket")
    
    st.markdown("---")
    
    col1, col2, col3, col4 = st.columns(4)
    with col1:
        if st.button("🔊 Play Sound", use_container_width=True):
            post("action/sound")
    with col2:
        if st.button("🔇 Mute", use_container_width=True):
            post("action/mute")
    with col3:
        if st.button("🏠 Home Screen", use_container_width=True):
            post("action/home")
    with col4:
        if st.button("🔄 Refresh Data", use_container_width=True):
            log("Refreshing data...")
            st.rerun()

# --- SETTINGS ---
with tab3:
    col1, col2 = st.columns(2)
    
    with col1:
        st.markdown("### 🕐 Display Settings")
        time_24hr = st.toggle("Use 24-hour time format")
        if st.button("Apply Time Format"):
            log(f"Setting time format: 24hr={time_24hr}")
            post("settings/time", {"format_24hr": time_24hr})
            st.success("✅ Time format updated!")
    
    with col2:
        st.markdown("### 👤 User Profile")
        user_name = st.text_input("Your Name", "Aravind")
        user_age = st.number_input("Age", 1, 100, 25)
        if st.button("💾 Save Profile", use_container_width=True):
            log(f"Saving profile: {user_name}, {user_age}")
            post("settings/user", {"name": user_name, "age": user_age})
            st.success("✅ Profile saved!")

# --- ANALYTICS ---
with tab4:
    st.markdown("### 📊 Real-Time Sensor Data")
    
    if st.session_state.last_sensors:
        col1, col2, col3 = st.columns(3)
        with col1:
            st.metric("🌡️ Temperature", f"{st.session_state.last_sensors.get('temp', '--')}°C")
        with col2:
            st.metric("💧 Humidity", f"{st.session_state.last_sensors.get('hum', '--')}%")
        with col3:
            st.metric("☀️ Light", f"{st.session_state.last_sensors.get('lux', '--')} lux")
    else:
        st.info("📡 Connect to ESP32 to see live sensor data.")

# --- LOG MONITOR ---
with tab5:
    st.markdown("### 📋 Global Log Monitor")
    
    col1, col2 = st.columns([4, 1])
    with col2:
        if st.button("🗑️ Clear Log", use_container_width=True):
            st.session_state.log_messages.clear()
            st.rerun()
    
    if st.session_state.log_messages:
        log_html = "<div class='log-container'>"
        for msg in list(st.session_state.log_messages)[-50:]:
            if "[ERROR]" in msg:
                log_html += f"<div class='log-error'>{msg}</div>"
            elif "[WARN]" in msg:
                log_html += f"<div class='log-warn'>{msg}</div>"
            else:
                log_html += f"<div class='log-info'>{msg}</div>"
        log_html += "</div>"
        st.markdown(log_html, unsafe_allow_html=True)
    else:
        st.info("No logs yet. Interact with the dashboard to see activity.")

# --- FOOTER ---
st.markdown("---")
st.markdown(f"""
<div style="text-align: center; color: #666; font-size: 0.9rem;">
    <p>
        <strong>ESP32-S3 AI Smart Hub</strong> • DigiKey Innovation Challenge 2026<br>
        Built with ❤️ by Circuit Digest • Powered by Google Gemini AI
    </p>
    <p style="font-size: 0.8rem; color: #555;">
        Last updated: {datetime.now().strftime('%H:%M:%S')} • Device: {st.session_state.esp32_ip}
    </p>
</div>
""", unsafe_allow_html=True)
