#!/usr/bin/env python3
import json
import hashlib
import os
import subprocess
import sys
import threading
import time
import urllib.request
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock


HOST = os.environ.get("ALGORITHM_AI_HOST", "127.0.0.1")
PORT = int(os.environ.get("ALGORITHM_AI_PORT", "7867"))
MODEL_PATH = os.environ.get("ALGORITHM_AI_MODEL_PATH", os.path.join(os.path.dirname(__file__), "models", "model.gguf"))
MODEL_URL = os.environ.get("ALGORITHM_AI_MODEL_URL", "")
TEMPLATE = os.environ.get("ALGORITHM_AI_TEMPLATE", "auto").lower()
THREADS = int(os.environ.get("ALGORITHM_AI_THREADS", "1"))
CTX = int(os.environ.get("ALGORITHM_AI_CTX", "512"))
MAX_TOKENS = int(os.environ.get("ALGORITHM_AI_MAX_TOKENS", "32"))
TEMPERATURE = float(os.environ.get("ALGORITHM_AI_TEMPERATURE", "0.65"))
TTS_ENABLED = os.environ.get("ALGORITHM_AI_TTS_ENABLED", "1") != "0"
TTS_API_KEY = os.environ.get("ELEVENLABS_API_KEY", "")
TTS_VOICE_ID = os.environ.get("ELEVENLABS_VOICE_ID", "")
TTS_MODEL = os.environ.get("ELEVENLABS_MODEL", "eleven_multilingual_v2")
TTS_OUTPUT_FORMAT = os.environ.get("ELEVENLABS_OUTPUT_FORMAT", "pcm_16000")
TTS_SAMPLE_RATE = int(os.environ.get("ELEVENLABS_SAMPLE_RATE", "16000"))
TTS_STABILITY = float(os.environ.get("ELEVENLABS_STABILITY", "0.6"))
TTS_SIMILARITY = float(os.environ.get("ELEVENLABS_SIMILARITY", "0.8"))
TTS_STYLE = float(os.environ.get("ELEVENLABS_STYLE", "0.86"))
TTS_SPEAKER_BOOST = os.environ.get("ELEVENLABS_SPEAKER_BOOST", "1") != "0"
TTS_CSTRIKE_ROOT = os.environ.get("ALGORITHM_AI_CSTRIKE_ROOT", "")
TTS_SOUND_DIR = os.environ.get("ALGORITHM_AI_TTS_SOUND_DIR", "sound/ebot_tts")
TTS_QUEUE_FILE = os.environ.get("ALGORITHM_AI_TTS_QUEUE_FILE", "")

SYSTEM_PROMPT = (
    "You are a normal human player on a Counter-Strike 1.6 zombie server. "
    "Never say that you are an AI, bot, language model, program, or that you have no feelings. "
    "Act like a casual player: friendly, brief, imperfect, and natural. "
    "If asked who you are, answer like a human player with a nickname. "
    "If asked what you feel, answer with simple human-like emotions. "
    "Use the same language as the player. If the player writes in Russian, reply only in Russian, not Bulgarian or Ukrainian. "
    "Answer in one short sentence, 10 to 18 words maximum."
)

SERVER_KNOWLEDGE = (
    "Server knowledge you know as a regular player:\n"
    "- To plant a lasermine: buy it with /lm, aim at a wall, then use the player's bind, usually bind p +setlaser.\n"
    "- If asked about laser mines in Russian, say: use /lm, aim at the wall, press bind p +setlaser.\n"
    "- To place a turret: buy the turret from extra items, then place it from its item/menu flow.\n"
    "- If asked about turrets in Russian, say: buy it in extra items and then place it from the menu.\n"
)

llm = None
llm_lock = Lock()
tts_lock = Lock()


def resolve_cstrike_root():
    if TTS_CSTRIKE_ROOT:
        return TTS_CSTRIKE_ROOT

    root = os.path.abspath(os.path.dirname(__file__))
    for _ in range(6):
        if os.path.basename(root).lower() == "cstrike":
            return root
        root = os.path.dirname(root)
    return ""


def tts_paths(text):
    cstrike_root = resolve_cstrike_root()
    if not cstrike_root:
        return "", "", ""

    sound_dir_rel = TTS_SOUND_DIR.replace("\\", "/").strip("/")
    sound_dir_abs = os.path.join(cstrike_root, *sound_dir_rel.split("/"))
    os.makedirs(sound_dir_abs, exist_ok=True)

    digest = hashlib.sha1(text.encode("utf-8", errors="ignore")).hexdigest()[:16]
    sound_rel = f"{sound_dir_rel}/{digest}.wav"
    sound_abs = os.path.join(cstrike_root, *sound_rel.split("/"))
    queue_file = TTS_QUEUE_FILE or os.path.join(cstrike_root, "addons", "amxmodx", "data", "ebot_tts_queue.ini")
    os.makedirs(os.path.dirname(queue_file), exist_ok=True)
    return sound_rel, sound_abs, queue_file


def write_pcm_wav(path, pcm):
    with wave.open(path, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(TTS_SAMPLE_RATE)
        wav.writeframes(pcm)


def generate_tts_file(text):
    sound_rel, sound_abs, queue_file = tts_paths(text)
    if not sound_rel:
        print("[algorithmAI] TTS skipped: cstrike root is not configured")
        return

    if not os.path.exists(sound_abs):
        url = f"https://api.elevenlabs.io/v1/text-to-speech/{TTS_VOICE_ID}"
        body = json.dumps({
            "text": text,
            "model_id": TTS_MODEL,
            "voice_settings": {
                "stability": TTS_STABILITY,
                "similarity_boost": TTS_SIMILARITY,
                "style": TTS_STYLE,
                "use_speaker_boost": TTS_SPEAKER_BOOST,
            },
        }).encode("utf-8")

        request = urllib.request.Request(
            f"{url}?output_format={TTS_OUTPUT_FORMAT}",
            data=body,
            headers={
                "xi-api-key": TTS_API_KEY,
                "Content-Type": "application/json",
            },
            method="POST",
        )
        with urllib.request.urlopen(request, timeout=60) as response:
            pcm = response.read()
        write_pcm_wav(sound_abs, pcm)
        print(f"[algorithmAI] TTS saved {sound_abs}")

    with tts_lock:
        with open(queue_file, "a", encoding="utf-8", newline="\n") as queue:
            queue.write(f"{sound_rel}\n")
    print(f"[algorithmAI] TTS queued {sound_rel}")


def queue_tts(text):
    text = normalize_reply(text)
    if not TTS_ENABLED or not text:
        return
    if not TTS_API_KEY or not TTS_VOICE_ID:
        print("[algorithmAI] TTS skipped: ELEVENLABS_API_KEY or ELEVENLABS_VOICE_ID is missing")
        return

    def worker():
        try:
            generate_tts_file(text)
        except Exception as exc:
            print(f"[algorithmAI] TTS failed: {exc}")

    threading.Thread(target=worker, daemon=True).start()


def ensure_model():
    if os.path.exists(MODEL_PATH) or not MODEL_URL:
        return

    os.makedirs(os.path.dirname(MODEL_PATH), exist_ok=True)
    print(f"[algorithmAI] downloading model to {MODEL_PATH}")
    urllib.request.urlretrieve(MODEL_URL, MODEL_PATH)


def init_model():
    global llm
    ensure_model()
    if not os.path.exists(MODEL_PATH):
        print("[algorithmAI] no GGUF model found; fallback replies are enabled")
        return

    try:
        from llama_cpp import Llama
    except Exception as exc:
        print(f"[algorithmAI] llama-cpp-python unavailable: {exc}")
        if os.environ.get("ALGORITHM_AI_AUTO_INSTALL", "1") != "0":
            try:
                print("[algorithmAI] installing llama-cpp-python")
                subprocess.check_call([sys.executable, "-m", "pip", "install", "llama-cpp-python"])
                from llama_cpp import Llama
            except Exception as install_exc:
                print(f"[algorithmAI] install failed: {install_exc}")
                print("[algorithmAI] fallback replies are enabled")
                return
        else:
            print("[algorithmAI] fallback replies are enabled")
            return

    print(f"[algorithmAI] loading {MODEL_PATH}")
    llm = Llama(
        model_path=MODEL_PATH,
        n_ctx=CTX,
        n_threads=THREADS,
        n_batch=64,
        verbose=False,
    )
    print("[algorithmAI] model ready")


def fallback_reply(player, message):
    msg = (message or "").strip()
    if "?" in msg:
        return f"{player}, \u0441\u0435\u043a\u0443\u043d\u0434\u0443, \u044f \u0447\u0443\u0442\u044c \u0437\u0430\u0432\u0438\u0441."
    return "\u0412\u0438\u0436\u0443 \u0447\u0430\u0442, \u0441\u0435\u0439\u0447\u0430\u0441 \u043e\u0442\u0432\u0435\u0447\u0443."


def has_cyrillic(text):
    return any("\u0400" <= ch <= "\u04ff" for ch in text or "")


def normalize_reply(text):
    text = " ".join((text or "").replace("\r", " ").replace("\n", " ").split())
    if not text:
        return ""

    sentence_end = min([idx for idx in (text.find("."), text.find("!"), text.find("?")) if idx >= 0] or [-1])
    if sentence_end >= 0:
        text = text[: sentence_end + 1]

    words = text.split()
    if len(words) > 15:
        text = " ".join(words[:15]).rstrip(" ,;:")

    if len(text.encode("utf-8", errors="ignore")) > 150:
        out = []
        size = 0
        for word in text.split():
            word_size = len(word.encode("utf-8", errors="ignore")) + (1 if out else 0)
            if size + word_size > 150:
                break
            out.append(word)
            size += word_size
        text = " ".join(out)

    return text.strip().strip('"')


def russian_fallback(message):
    msg = (message or "").lower()
    if "\u043a\u0442\u043e" in msg:
        return "\u042f \u043e\u0431\u044b\u0447\u043d\u044b\u0439 \u0438\u0433\u0440\u043e\u043a, \u043f\u0440\u043e\u0441\u0442\u043e \u0431\u0435\u0433\u0430\u044e \u0442\u0443\u0442 \u0441 \u0432\u0430\u043c\u0438."
    if "\u0447\u0443\u0432" in msg:
        return "\u0414\u0430 \u043d\u043e\u0440\u043c\u0430\u043b\u044c\u043d\u043e, \u043d\u0435\u043c\u043d\u043e\u0433\u043e \u043d\u0430\u043f\u0440\u044f\u0436\u043d\u043e, \u0437\u043e\u043c\u0431\u0438 \u0440\u044f\u0434\u043e\u043c."
    return "\u0414\u0430 \u044f \u0442\u0443\u0442, \u0438\u0433\u0440\u0430\u044e \u043f\u043e\u0442\u0438\u0445\u043e\u043d\u044c\u043a\u0443."


def generate_reply(player, message):
    player = (player or "player")[:32]
    message = (message or "")[:240]

    if llm is None:
        return fallback_reply(player, message)

    template = TEMPLATE
    if template == "auto":
        model_name = os.path.basename(MODEL_PATH).lower()
        template = "qwen" if "qwen" in model_name else "tinyllama"

    if template == "qwen":
        language_rule = "The player wrote in Russian. Reply only in Russian.\n" if has_cyrillic(message) else ""
        prompt = (
            f"<|im_start|>system\n{SYSTEM_PROMPT}<|im_end|>\n"
            f"<|im_start|>system\n{SERVER_KNOWLEDGE}<|im_end|>\n"
            f"<|im_start|>user\n{language_rule}{player}: {message}<|im_end|>\n"
            f"<|im_start|>assistant\n"
        )
        stop = ["<|im_end|>", "<|im_start|>", "\n\n"]
    else:
        language_rule = "The player wrote in Russian. Reply only in Russian.\n" if has_cyrillic(message) else ""
        prompt = (
            f"<|system|>\n{SYSTEM_PROMPT}\n{SERVER_KNOWLEDGE}</s>\n"
            f"<|user|>\n{language_rule}{player}: {message}</s>\n"
            f"<|assistant|>\n"
        )
        stop = ["</s>", "<|user|>", "<|system|>", "\n\n"]

    try:
        llm_lock.acquire()
        result = llm(
            prompt,
            max_tokens=MAX_TOKENS,
            temperature=TEMPERATURE,
            stop=stop,
        )
    except Exception as exc:
        print(f"[algorithmAI] generation failed: {exc}")
        return fallback_reply(player, message)
    finally:
        llm_lock.release()

    text = normalize_reply(result["choices"][0]["text"])
    if has_cyrillic(message) and not has_cyrillic(text):
        text = russian_fallback(message)
    return text or fallback_reply(player, message)


class Handler(BaseHTTPRequestHandler):
    server_version = "EBOT-algorithmAI/0.1"

    def do_GET(self):
        if self.path != "/health":
            self.send_error(404)
            return

        self.send_json({"ok": True, "model_loaded": llm is not None, "time": time.time()})

    def do_POST(self):
        if self.path != "/chat":
            self.send_error(404)
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(min(length, 4096))
        try:
            try:
                decoded = raw.decode("utf-8")
            except UnicodeDecodeError:
                decoded = raw.decode("cp1251", errors="replace")
            payload = json.loads(decoded)
        except Exception:
            self.send_error(400)
            return

        reply = generate_reply(payload.get("player", "player"), payload.get("message", ""))
        queue_tts(reply)
        self.send_json({"reply": reply})

    def send_json(self, payload):
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            print("[algorithmAI] client disconnected before reply was sent")

    def log_message(self, fmt, *args):
        print(f"[algorithmAI] {self.address_string()} {fmt % args}")


def main():
    init_model()
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"[algorithmAI] listening on http://{HOST}:{PORT}")
    server.serve_forever()


if __name__ == "__main__":
    main()
