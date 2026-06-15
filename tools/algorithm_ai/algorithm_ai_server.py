#!/usr/bin/env python3
import json
import os
import subprocess
import sys
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


HOST = os.environ.get("ALGORITHM_AI_HOST", "127.0.0.1")
PORT = int(os.environ.get("ALGORITHM_AI_PORT", "7867"))
MODEL_PATH = os.environ.get("ALGORITHM_AI_MODEL_PATH", os.path.join(os.path.dirname(__file__), "models", "model.gguf"))
MODEL_URL = os.environ.get("ALGORITHM_AI_MODEL_URL", "")
TEMPLATE = os.environ.get("ALGORITHM_AI_TEMPLATE", "auto").lower()
THREADS = int(os.environ.get("ALGORITHM_AI_THREADS", "1"))
CTX = int(os.environ.get("ALGORITHM_AI_CTX", "512"))
MAX_TOKENS = int(os.environ.get("ALGORITHM_AI_MAX_TOKENS", "64"))
TEMPERATURE = float(os.environ.get("ALGORITHM_AI_TEMPERATURE", "0.65"))

SYSTEM_PROMPT = (
    "You are a normal human player on a Counter-Strike 1.6 zombie server. "
    "Never say that you are an AI, bot, language model, program, or that you have no feelings. "
    "Act like a casual player: friendly, brief, imperfect, and natural. "
    "If asked who you are, answer like a human player with a nickname. "
    "If asked what you feel, answer with simple human-like emotions. "
    "Use the same language as the player. If the player writes in Russian, reply only in Russian. "
    "Keep answers under 180 characters."
)

llm = None


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
        return f"{player}, AI is warming up. I can read you, model is not loaded yet."
    return "I heard you. Model is not loaded yet, but the chat bridge works."


def has_cyrillic(text):
    return any("\u0400" <= ch <= "\u04ff" for ch in text or "")


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
            f"<|im_start|>user\n{language_rule}{player}: {message}<|im_end|>\n"
            f"<|im_start|>assistant\n"
        )
        stop = ["<|im_end|>", "<|im_start|>", "\n\n"]
    else:
        language_rule = "The player wrote in Russian. Reply only in Russian.\n" if has_cyrillic(message) else ""
        prompt = (
            f"<|system|>\n{SYSTEM_PROMPT}</s>\n"
            f"<|user|>\n{language_rule}{player}: {message}</s>\n"
            f"<|assistant|>\n"
        )
        stop = ["</s>", "<|user|>", "<|system|>", "\n\n"]

    result = llm(
        prompt,
        max_tokens=MAX_TOKENS,
        temperature=TEMPERATURE,
        stop=stop,
    )
    text = result["choices"][0]["text"].strip()
    return " ".join(text.split())[:180] or fallback_reply(player, message)


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
            payload = json.loads(raw.decode("utf-8", errors="replace"))
        except Exception:
            self.send_error(400)
            return

        reply = generate_reply(payload.get("player", "player"), payload.get("message", ""))
        self.send_json({"reply": reply})

    def send_json(self, payload):
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt, *args):
        print(f"[algorithmAI] {self.address_string()} {fmt % args}")


def main():
    init_model()
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"[algorithmAI] listening on http://{HOST}:{PORT}")
    server.serve_forever()


if __name__ == "__main__":
    main()
