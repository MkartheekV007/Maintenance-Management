"""Create the database and load the CSVs in ./data.

    ADMIN_PASSWORD='choose-one' python seed.py

All admins start with ADMIN_PASSWORD and are made to choose their own
on first login.
Running it again is safe: it never overwrites existing admins, students or stock.
"""
import csv, getpass, os, sqlite3, sys
from datetime import datetime
from werkzeug.security import generate_password_hash

BASE = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.environ.get("MAINT_DB", os.path.join(BASE, "maintenance.db"))

SCHEMA = """
CREATE TABLE IF NOT EXISTS admins(id INTEGER PRIMARY KEY, name TEXT NOT NULL, pw_hash TEXT NOT NULL,
  must_change INTEGER NOT NULL DEFAULT 1);
CREATE TABLE IF NOT EXISTS duties(date TEXT PRIMARY KEY, member TEXT, member2 TEXT, member3 TEXT, day TEXT);
CREATE TABLE IF NOT EXISTS stock(key TEXT PRIMARY KEY, item TEXT NOT NULL, qty INTEGER NOT NULL DEFAULT 0);
CREATE TABLE IF NOT EXISTS students(regno INTEGER PRIMARY KEY, name TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS complaints(
  id INTEGER PRIMARY KEY, student TEXT, regno INTEGER, type TEXT, room TEXT, details TEXT,
  date TEXT, day TEXT, duty TEXT, status TEXT NOT NULL DEFAULT 'pending',
  cost REAL DEFAULT 0, items_used TEXT, done_on TEXT, done_by TEXT);
CREATE TABLE IF NOT EXISTS srd_requests(
  id INTEGER PRIMARY KEY AUTOINCREMENT, srd TEXT, item TEXT, date TEXT, day TEXT, created TEXT);
"""

def read_csv(name):
    path = os.path.join(BASE, "data", name)
    if not os.path.exists(path):
        return []
    with open(path, newline="", encoding="utf-8-sig") as f:
        rows = [[c.strip() for c in r] for r in csv.reader(f) if any(c.strip() for c in r)]
    return rows[1:]  # skip header

def init_db(path, admin_password):
    db = sqlite3.connect(path)
    db.executescript(SCHEMA)
    for name, ident, *_ in read_csv("admins.csv"):
        db.execute("INSERT OR IGNORE INTO admins(id,name,pw_hash) VALUES(?,?,?)",
                   (int(ident), name, generate_password_hash(admin_password)))
    for r in read_csv("duties.csv"):
        member, m2, m3, day, d = (r + [""] * 5)[:5]
        iso = datetime.strptime(d, "%m/%d/%y").strftime("%Y-%m-%d")
        db.execute("INSERT OR REPLACE INTO duties VALUES(?,?,?,?,?)",
                   (iso, member or None, m2 or None, m3 or None, day))
    for regno, name, *_ in read_csv("students.csv"):
        db.execute("INSERT OR IGNORE INTO students VALUES(?,?)", (int(regno), name))
    for item, qty, *_ in read_csv("stock.csv"):
        db.execute("INSERT OR IGNORE INTO stock VALUES(?,?,?)", (item.lower(), item, int(qty or 0)))
    db.commit()
    db.close()

if __name__ == "__main__":
    pw = os.environ.get("ADMIN_PASSWORD") or getpass.getpass("Starting password for all admins: ")
    if len(pw) < 8:
        sys.exit("Password must be at least 8 characters.")
    init_db(DB_PATH, pw)
    print("Database ready:", DB_PATH)
