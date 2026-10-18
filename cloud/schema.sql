-- Key Compass cloud sync: the tables in the "keycompass-sync" D1 database.
-- Safe to run again (IF NOT EXISTS). Apply with:
--   npx wrangler d1 execute keycompass-sync --remote --file cloud/schema.sql

-- One row per sync code. 'id' is a SHA-256 of the code - the code itself is never stored.
-- 'seq' counts every change in the account, so a device can ask for "everything after change 41".
CREATE TABLE IF NOT EXISTS accounts (
    id        TEXT PRIMARY KEY,
    seq       INTEGER NOT NULL DEFAULT 0,
    created   INTEGER NOT NULL,
    last_seen INTEGER NOT NULL
);

-- Songs (kind 'song', id = the song's own id) and the starred progressions (kind 'favourites').
-- 'rev' goes up by one with every saved change; a device must say which rev it changed (no lost edits).
-- Deleted songs stay as a row with deleted = 1 and no data, so other devices learn about the delete.
CREATE TABLE IF NOT EXISTS items (
    account  TEXT NOT NULL,
    id       TEXT NOT NULL,
    kind     TEXT NOT NULL,
    name     TEXT NOT NULL DEFAULT '',
    rev      INTEGER NOT NULL,
    seq      INTEGER NOT NULL,
    modified INTEGER NOT NULL,
    device   TEXT NOT NULL DEFAULT '',
    deleted  INTEGER NOT NULL DEFAULT 0,
    data     TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (account, id)
);

CREATE INDEX IF NOT EXISTS items_by_seq ON items (account, seq);
