#!/usr/bin/env python3
import sys
import os
import json
import hashlib

def main():
    if len(sys.argv) < 3:
        print("Usage: generate_manifest.py <model_file> <output_manifest_json> [model_id]")
        sys.exit(1)
    
    model_path = sys.argv[1]
    manifest_path = sys.argv[2]
    model_id = sys.argv[3] if len(sys.argv) > 3 else "qwen2.5-0.5b-instruct-q4_k_m"

    sha256 = hashlib.sha256()
    size_bytes = os.path.getsize(model_path)
    with open(model_path, "rb") as f:
        while chunk := f.read(65536):
            sha256.update(chunk)
    
    file_sha = sha256.hexdigest()
    model_basename = os.path.basename(model_path)

    manifest = {
        "version": 1,
        "created_utc": "2026-08-12T00:00:00Z",
        "model_id": model_id,
        "files": [
            {
                "path": model_basename,
                "sha256": file_sha,
                "size_bytes": size_bytes
            }
        ],
        "ed25519_signature": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    }

    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    
    print(f"Manifest created at {manifest_path} for {model_basename} (SHA-256: {file_sha}, Size: {size_bytes} bytes)")

if __name__ == "__main__":
    main()
