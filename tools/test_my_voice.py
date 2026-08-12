#!/usr/bin/env python3
# tools/test_my_voice.py
#
# Native C++ Speech-to-Text & GOAP Intent / LLM execution tool.

import sys
import os
import subprocess

def main():
    wav_path = "my_voice.wav"
    if not os.path.exists(wav_path):
        print(f"Error: '{wav_path}' not found!")
        print("Please record your voice first using:")
        print("  ffmpeg -f pulse -i default -t 5 -ar 16000 -ac 1 my_voice.wav")
        sys.exit(1)

    print(f"[+] Found {wav_path} (5.0s PCM 16kHz audio).")
    print("[+] Running native C++ whisper.cpp STT model...")

    whisper_cli = "./third_party/whisper.cpp/build/bin/whisper-cli"
    model_path = "data/models/stt/whisper-tiny.en.bin"

    if not os.path.exists(whisper_cli) or not os.path.exists(model_path):
        print("Error: whisper-cli or model not found!")
        sys.exit(1)

    cmd = [whisper_cli, "-m", model_path, "-f", wav_path, "--no-timestamps", "-nt"]
    res = subprocess.run(cmd, capture_output=True, text=True)

    # Extract transcription line from whisper output
    lines = [line.strip() for line in res.stdout.splitlines() if line.strip() and not line.startswith("whisper_") and not line.startswith("system_info") and not line.startswith("main:") and not line.startswith("read_audio")]
    transcription = " ".join(lines).strip()

    print("\n=======================================================")
    print(f"  YOUR SPOKEN WORDS: \"{transcription}\"")
    print("=======================================================\n")

    if not transcription:
        print("[!] No speech recognized in recording.")
        return

    # Pass transcribed text into monday-core with clean stderr filtering
    print("[+] Passing spoken text into Monday (GOAP / LLM)...")
    core_cmd = ["./build-debug/monday-core", "--text-only", "--config", "config.toml"]
    proc = subprocess.Popen(core_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    
    # Send transcription and close stdin so monday-core knows input ended
    proc.stdin.write(transcription + "\nexit\n")
    proc.stdin.flush()
    proc.stdin.close()

    print("\n[+] MONDAY RESPONSE:")
    for line in proc.stdout:
        line_str = line.strip()
        if line_str and not line_str.startswith("create_tensor") and not line_str.startswith("load_tensors") and not line_str.startswith("llama_") and not line_str.startswith("sched_") and not line_str.startswith("graph_") and not line_str.startswith("repack:") and not line_str.startswith("done_getting") and not line_str.startswith("register_") and not line_str.startswith("print_info") and not line_str.startswith("init_tokenizer") and not line_str.startswith("load:") and not line_str.startswith("special tokens") and not line_str.startswith("token to piece") and not line_str.startswith("set_abort_callback") and not line_str.startswith("resolve_fused_ops"):
            if "Monday (text-only mode)" in line_str or "Type 'exit'" in line_str:
                continue
            if line_str.startswith("You: Monday:"):
                print("  " + line_str[5:])  # Clean up prompt prefix
            elif line_str.startswith("Monday:"):
                print("  " + line_str)
            elif not line_str.startswith("You:"):
                print("  " + line_str)

    proc.wait()
    print()

if __name__ == "__main__":
    main()
