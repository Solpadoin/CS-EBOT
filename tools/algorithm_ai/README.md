# EBOT algorithmAI chat helper

This is an experimental local LLM bridge for EBOT chat.

EBOT watches player chat when `ebot_ai_chat 1` is enabled. If a message looks like a question or mentions a bot, EBOT sends it to this local helper:

```text
POST http://127.0.0.1:7867/chat
```

The helper returns:

```json
{"reply":"short bot answer"}
```

Then EBOT picks one alive bot and sends the answer through `FakeClientCommand(bot, "say ...")`.

## Start on Windows

```powershell
cd "C:\Users\Admin\Documents\ZM 4.3\_downloads\CS-EBOT-clean\tools\algorithm_ai"
powershell -ExecutionPolicy Bypass -File .\start_algorithm_ai.ps1
```

In the server console:

```text
ebot_ai_chat 1
```

## Model

Put a small GGUF model at:

```text
tools/algorithm_ai/models/model.gguf
```

Or set:

```powershell
$env:ALGORITHM_AI_MODEL_PATH="C:\path\to\small-model.gguf"
```

Optional auto-download:

```powershell
$env:ALGORITHM_AI_MODEL_URL="https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q3_k_m.gguf"
```

Current local test model:

```text
Qwen2.5-3B-Instruct-GGUF / qwen2.5-3b-instruct-q3_k_m.gguf
```

For the current server budget, use the 3B Q3_K_M model with `ALGORITHM_AI_THREADS=2`, `ALGORITHM_AI_CTX=512`, and `ALGORITHM_AI_MAX_TOKENS=32`. The chat bridge trims replies to one short in-game sentence, so longer model output does not overflow the CS 1.6 chat line.

On Linux, enforce the CPU budget with systemd. A ready template is included as:

```text
tools/algorithm_ai/ebot-algorithm-ai.service
```

It uses `CPUQuota=30%` and `MemoryMax=1900M`; adjust `WorkingDirectory` and `ExecStart` to the real server path before installing it.

When EBOT loads, the DLL can autostart this helper from `addons/ebot/algorithm_ai`. If `ALGORITHM_AI_MODEL_URL` is set and `models/model.gguf` is missing, the helper downloads the model. If `llama-cpp-python` is missing, it tries to install it once via pip and falls back gracefully if that fails.

GGUF files are intentionally ignored by Git because they are large. The active local model path is:

```text
tools/algorithm_ai/models/model.gguf
```

If no model is loaded, the helper still returns fallback replies so the EBOT chat bridge can be tested immediately.
