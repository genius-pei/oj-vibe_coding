#!/usr/bin/env python3
"""Insert (or replace) a user with a bcrypt-hashed password into minioj.users.

Default action: create admin / admin123 (matches backend bcrypt-12 settings).

If the target username already exists, it is DELETED (along with its sessions)
and re-inserted with the new password hash. Pass --keep-existing to update in
place instead of delete+insert.

Usage:
    python3 create_admin.py                            # default: admin / admin123
    python3 create_admin.py --username alice --password P4ssword! --role user
    python3 create_admin.py --check admin admin123     # verify a hash matches a password

Requires:
    apt-get install -y python3-pymysql    # pure-Python, no build deps
"""

from __future__ import annotations

import argparse
import configparser
import crypt
import re
import secrets
import sys
import warnings
from pathlib import Path

import pymysql

warnings.filterwarnings("ignore", category=DeprecationWarning)  # crypt() deprecation

DEFAULT_CNF = Path.home() / ".mysql" / "minioj.cnf"
USERNAME_RE = re.compile(r"^[A-Za-z0-9_]{3,20}$")
BCRYPT_SALT_ALPHABET = "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
BCRYPT_ROUNDS = 12


def bcrypt_hash(password: str, rounds: int = BCRYPT_ROUNDS) -> str:
    """Hash `password` with bcrypt via glibc crypt(). Returns full $2b$rounds$salt22$hash31."""
    if not 4 <= rounds <= 31:
        raise ValueError("rounds must be in [4, 31]")
    salt_body = "".join(secrets.choice(BCRYPT_SALT_ALPHABET) for _ in range(22))
    salt = f"$2b${rounds:02d}${salt_body}"
    result = crypt.crypt(password, salt)
    if not result or not result.startswith("$2b$"):
        raise RuntimeError(
            f"glibc crypt() does not support bcrypt here (got {result!r}). "
            "Need a glibc-based Python build."
        )
    return result


def verify_hash(password: str, stored_hash: str) -> bool:
    """Re-hash with the stored hash as salt; glibc crypt() takes the first
    29 chars (`$2b$<rounds>$<22 salt chars>`) and ignores the rest."""
    if not stored_hash or not stored_hash.startswith("$2"):
        return False
    candidate = crypt.crypt(password, stored_hash)
    return candidate is not None and candidate == stored_hash


def validate_credentials(username: str, password: str) -> None:
    if not USERNAME_RE.match(username):
        sys.exit(f"error: username {username!r} invalid (need 3-20 [A-Za-z0-9_])")
    if not 8 <= len(password) <= 64:
        sys.exit("error: password must be 8-64 characters long")
    if not any(c.isalpha() for c in password):
        sys.exit("error: password must contain at least one letter")
    if not any(c.isdigit() for c in password):
        sys.exit("error: password must contain at least one digit")


def open_conn(cnf_path: Path):
    if not cnf_path.exists():
        sys.exit(f"error: cnf file not found: {cnf_path}")
    cp = configparser.ConfigParser()
    cp.read(cnf_path)
    section = cp["client"]
    return pymysql.connect(
        host=section.get("host", "127.0.0.1"),
        user=section["user"],
        password=section["password"],
        database=section.get("database"),
        charset="utf8mb4",
        autocommit=False,
    )


def cmd_create(args) -> int:
    validate_credentials(args.username, args.password)
    conn = open_conn(Path(args.cnf))
    cur = conn.cursor()
    try:
        cur.execute("SELECT id, role FROM users WHERE username = %s", (args.username,))
        existing_row = cur.fetchone()

        password_hash = bcrypt_hash(args.password)

        if existing_row:
            existing_id, existing_role = existing_row
            if args.keep_existing:
                cur.execute(
                    "UPDATE users SET password_hash = %s, role = %s WHERE id = %s",
                    (password_hash, args.role, existing_id),
                )
                conn.commit()
                action = "updated"
                uid = existing_id
            else:
                # delete cascades to sessions (FK ON DELETE CASCADE)
                cur.execute("DELETE FROM users WHERE id = %s", (existing_id,))
                deleted = cur.rowcount
                cur.execute(
                    "INSERT INTO users (username, password_hash, role) VALUES (%s, %s, %s)",
                    (args.username, password_hash, args.role),
                )
                conn.commit()
                uid = cur.lastrowid
                action = "replaced (deleted old + inserted new)"
                print(f"  note: deleted existing id={existing_id} role={existing_role!r} "
                      f"({deleted} row) and recreated")
        else:
            cur.execute(
                "INSERT INTO users (username, password_hash, role) VALUES (%s, %s, %s)",
                (args.username, password_hash, args.role),
            )
            conn.commit()
            action = "created"
            uid = cur.lastrowid

        cur.execute(
            "SELECT id, username, role, LEFT(password_hash, 7) AS algo "
            "FROM users WHERE id = %s",
            (uid,),
        )
        row = cur.fetchone()
        print(f"{action}: id={row[0]} username={row[1]!r} "
              f"role={row[2]!r} algo={row[3]}")
        print(f"  full bcrypt hash stored ({len(password_hash)} chars): "
              f"{password_hash[:29]}...")
    finally:
        cur.close()
        conn.close()
    return 0


def cmd_check(args) -> int:
    conn = open_conn(Path(args.cnf))
    cur = conn.cursor()
    try:
        cur.execute(
            "SELECT password_hash FROM users WHERE username = %s", (args.username,)
        )
        row = cur.fetchone()
        if not row:
            print(f"user {args.username!r} not found")
            return 2
        ok = verify_hash(args.password, row[0])
        print(f"verify {args.username}: {'OK' if ok else 'MISMATCH'}")
        return 0 if ok else 1
    finally:
        cur.close()
        conn.close()


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Insert or replace a user with bcrypt-hashed password."
    )
    p.add_argument("--username", default="admin")
    p.add_argument("--password", default="admin123")
    p.add_argument("--role", default="admin", choices=["admin", "user"])
    p.add_argument("--cnf", default=str(DEFAULT_CNF),
                   help="MySQL client options file")
    p.add_argument("--keep-existing", action="store_true",
                   help="UPDATE in place instead of DELETE+INSERT when user exists")
    sub = p.add_subparsers(dest="cmd")
    sub.add_parser("check", help="verify a hash matches a password")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.cmd == "check":
        return cmd_check(args)
    return cmd_create(args)


if __name__ == "__main__":
    sys.exit(main())