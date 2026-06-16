$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Venv = Join-Path $Root ".venv"
$Python = Join-Path $Venv "Scripts\python.exe"

if (!(Test-Path $Python)) {
    python -m venv $Venv
}

& $Python -m pip install --upgrade pip
& $Python -m pip install llama-cpp-python

if (!$env:ALGORITHM_AI_PORT) { $env:ALGORITHM_AI_PORT = "7867" }
if (!$env:ALGORITHM_AI_THREADS) { $env:ALGORITHM_AI_THREADS = "2" }
if (!$env:ALGORITHM_AI_CTX) { $env:ALGORITHM_AI_CTX = "512" }
if (!$env:ALGORITHM_AI_MAX_TOKENS) { $env:ALGORITHM_AI_MAX_TOKENS = "32" }
if (!$env:ALGORITHM_AI_TEMPLATE) { $env:ALGORITHM_AI_TEMPLATE = "qwen" }
if (!$env:ALGORITHM_AI_MODEL_URL) { $env:ALGORITHM_AI_MODEL_URL = "https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF/resolve/main/qwen2.5-3b-instruct-q3_k_m.gguf" }
if (!$env:ALGORITHM_AI_TTS_ENABLED) { $env:ALGORITHM_AI_TTS_ENABLED = "1" }
if (!$env:ELEVENLABS_OUTPUT_FORMAT) { $env:ELEVENLABS_OUTPUT_FORMAT = "pcm_16000" }
if (!$env:ELEVENLABS_SAMPLE_RATE) { $env:ELEVENLABS_SAMPLE_RATE = "16000" }

& $Python (Join-Path $Root "algorithm_ai_server.py")
