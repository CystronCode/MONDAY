-- monday/storage/schema_v1.sql
-- Database Schema Version 1

-- 1. Configuration Table
CREATE TABLE IF NOT EXISTS config (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- 2. Permission Grants Table (Architecture §3.7)
CREATE TABLE IF NOT EXISTS permission_grants (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    action_class TEXT    NOT NULL,  -- 'WRITE', 'READ_ONLY', 'DESTRUCTIVE'
    target_scope TEXT    NOT NULL,  -- directory or binary path
    granted_at   INTEGER NOT NULL,  -- Unix timestamp (seconds)
    expires_at   INTEGER NOT NULL,  -- granted_at + 86400 (24h TTL)
    session_id   TEXT    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_grants_target ON permission_grants(action_class, target_scope);

-- 3. Episodic Memory Table (Architecture §3.6)
CREATE TABLE IF NOT EXISTS episodic_memory (
    turn_id       INTEGER PRIMARY KEY,
    session_id    TEXT    NOT NULL,
    summary       TEXT    NOT NULL,
    conversation  TEXT    NOT NULL,
    hmac_sig      BLOB    NOT NULL,  -- 32-byte HMAC-SHA256 signature
    timestamp     INTEGER NOT NULL
);

-- 4. Semantic Memory Table (Architecture §3.6)
CREATE TABLE IF NOT EXISTS semantic_memory (
    chunk_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    doc_id        TEXT    NOT NULL,
    text_content  TEXT    NOT NULL,
    embedding     BLOB    NOT NULL,  -- Binary float vector array
    timestamp     INTEGER NOT NULL
);

-- 5. FTS5 Virtual Table for Text Retrieval (Architecture §2.6)
CREATE VIRTUAL TABLE IF NOT EXISTS fts_idx USING fts5(
    chunk_id UNINDEXED,
    text_content
);
