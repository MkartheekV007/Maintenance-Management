"""Maintenance Management System - web version (Flask + SQLite)."""
import os
import secrets
import sqlite3
from datetime import date, datetime
from functools import wraps

from flask import (Flask, abort, flash, g, redirect, render_template,
                   request, session, url_for)
from werkzeug.security import check_password_hash, generate_password_hash

BASE = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.environ.get("MAINT_DB", os.path.join(BASE, "maintenance.db"))
STUDENT_PASSWORD = os.environ.get("STUDENT_PASSWORD", "student123")
SRD_CODE = os.environ.get("SRD_CODE", "srd123")
COMPLAINT_TYPES = ["Plumbing", "Electric", "Carpentry"]

app = Flask(__name__)
app.secret_key = os.environ.get("SECRET_KEY", "dev-only-change-me")
app.config.update(SESSION_COOKIE_HTTPONLY=True, SESSION_COOKIE_SAMESITE="Lax")


# ---------- helpers ----------

def db():
    if "db" not in g:
        g.db = sqlite3.connect(app.config.get("DB", DB_PATH), timeout=10)
        g.db.row_factory = sqlite3.Row
    return g.db


@app.teardown_appcontext
def close_db(_exc):
    conn = g.pop("db", None)
    if conn:
        conn.close()


def today():
    try:
        from zoneinfo import ZoneInfo
        return datetime.now(ZoneInfo(os.environ.get("APP_TZ", "Asia/Kolkata"))).date()
    except Exception:
        return date.today()


def parse_day(text):
    """'2026-09-29' -> (date, 'Tuesday') or None."""
    try:
        d = datetime.strptime((text or "").strip(), "%Y-%m-%d").date()
        return d, d.strftime("%A")
    except ValueError:
        return None


def to_int(text, default=None):
    try:
        return int(str(text).strip())
    except (TypeError, ValueError):
        return default


def clean(text, limit=200):
    return " ".join((text or "").split())[:limit]


def duty_for(iso):
    row = db().execute("SELECT member FROM duties WHERE date=?", (iso,)).fetchone()
    return row["member"] if row and row["member"] else "Unassigned"


def stock_row(item):
    return db().execute("SELECT item, qty FROM stock WHERE key=?", (clean(item).lower(),)).fetchone()


def stock_qty(item):
    row = stock_row(item)
    return row["qty"] if row else 0


def csrf_token():
    if "csrf" not in session:
        session["csrf"] = secrets.token_hex(16)
    return session["csrf"]


app.jinja_env.globals["csrf_token"] = csrf_token


@app.before_request
def check_csrf():
    if request.method == "POST":
        expected = session.get("csrf")
        sent = request.form.get("csrf", "")
        if not expected or not secrets.compare_digest(sent, expected):
            abort(400)


@app.before_request
def force_password_change():
    """Admins on the starting password can only reach the change-password page."""
    if session.get("must_change") and request.endpoint not in (
            "set_password", "admin_password", "logout", "login", "static"):
        return redirect(url_for("set_password"))


def role_required(role):
    def deco(fn):
        @wraps(fn)
        def wrapper(*a, **kw):
            if session.get("role") != role:
                return redirect(url_for("login"))
            return fn(*a, **kw)
        return wrapper
    return deco


# ---------- login ----------

@app.route("/")
def home():
    role = session.get("role")
    return redirect(url_for(role) if role else url_for("login"))


@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        role = request.form.get("role")
        ident = clean(request.form.get("ident"), 60)
        password = request.form.get("password", "")
        name = None
        must_change = False

        if role == "student":
            reg = to_int(ident)
            if reg is not None and secrets.compare_digest(password, STUDENT_PASSWORD):
                row = db().execute("SELECT name FROM students WHERE regno=?", (reg,)).fetchone()
                name = row["name"] if row else "Guest Student"
                ident = reg
        elif role == "srd":
            if ident and secrets.compare_digest(password, SRD_CODE):
                name = ident
        elif role == "admin":
            admin_id = to_int(ident)
            row = db().execute("SELECT * FROM admins WHERE id=?", (admin_id,)).fetchone()
            if row and check_password_hash(row["pw_hash"], password):
                name, ident = row["name"], row["id"]
                must_change = bool(row["must_change"])

        if name:
            session.clear()
            session.update(role=role, name=name, ident=ident)
            if role == "admin" and must_change:
                session["must_change"] = True
                return redirect(url_for("set_password"))
            return redirect(url_for(role))
        flash("Those details don't match. Check the ID and password and try again.", "error")
    return render_template("login.html")


@app.route("/logout", methods=["POST"])
def logout():
    session.clear()
    return redirect(url_for("login"))


# ---------- student ----------

@app.route("/student")
@role_required("student")
def student():
    mine = db().execute("SELECT * FROM complaints WHERE regno=? ORDER BY id DESC",
                        (session["ident"],)).fetchall()
    return render_template("student.html", complaints=mine, types=COMPLAINT_TYPES,
                           today=today().isoformat())


@app.post("/student/complaint")
@role_required("student")
def student_complaint():
    parsed = parse_day(request.form.get("date"))
    ctype = request.form.get("type")
    room = clean(request.form.get("room"), 120)
    if not parsed or ctype not in COMPLAINT_TYPES or not room:
        flash("Choose a date, a type and say where the problem is.", "error")
        return redirect(url_for("student"))
    d, day = parsed
    duty = duty_for(d.isoformat())
    cur = db().execute(
        "INSERT INTO complaints(id,student,regno,type,room,details,date,day,duty) VALUES("
        "(SELECT MAX(100, COALESCE(MAX(id),0)+1) FROM complaints),?,?,?,?,?,?,?,?)",
        (session["name"], session["ident"], ctype, room,
         clean(request.form.get("details"), 300), d.strftime("%d-%m-%Y"), day, duty))
    db().commit()
    flash(f"{cur.lastrowid}|{duty}", "ticket")
    return redirect(url_for("student"))


# ---------- SRD ----------

@app.route("/srd")
@role_required("srd")
def srd():
    rows = db().execute("SELECT * FROM srd_requests ORDER BY id DESC LIMIT 50").fetchall()
    return render_template("srd.html", requests=rows, today=today().isoformat())


@app.post("/srd/request")
@role_required("srd")
def srd_request():
    parsed = parse_day(request.form.get("date"))
    item = clean(request.form.get("item"), 80)
    if not parsed or not item:
        flash("Choose a date and enter an item.", "error")
        return redirect(url_for("srd"))
    qty = stock_qty(item)
    if qty <= 0:
        flash(f"{item} is not in stock. Please contact Maintenance.", "error")
        return redirect(url_for("srd"))
    d, day = parsed
    db().execute("INSERT INTO srd_requests(srd,item,date,day,created) VALUES(?,?,?,?,?)",
                 (session["name"], item, d.strftime("%d-%m-%Y"), day,
                  datetime.now().strftime("%Y-%m-%d %H:%M")))
    db().commit()
    flash(f"Request saved. {item} is available (quantity {qty}).", "ok")
    return redirect(url_for("srd"))


# ---------- admin ----------

@app.route("/admin")
@role_required("admin")
def admin():
    q = db().execute
    return render_template(
        "admin.html",
        pending=q("SELECT * FROM complaints WHERE status='pending' ORDER BY id").fetchall(),
        done=q("SELECT * FROM complaints WHERE status='done' ORDER BY id DESC LIMIT 15").fetchall(),
        stock=q("SELECT * FROM stock ORDER BY item COLLATE NOCASE").fetchall(),
        requests=q("SELECT * FROM srd_requests ORDER BY id DESC LIMIT 100").fetchall())


@app.post("/admin/stock")
@role_required("admin")
def admin_stock():
    item = clean(request.form.get("item"), 80)
    qty = to_int(request.form.get("qty"))
    if not item or qty is None or not 0 <= qty <= 100000:
        flash("Enter an item name and a quantity of 0 or more.", "error")
        return redirect(url_for("admin") + "#stock")
    db().execute("INSERT INTO stock(key,item,qty) VALUES(?,?,?) "
                 "ON CONFLICT(key) DO UPDATE SET qty=qty+excluded.qty", (item.lower(), item, qty))
    db().commit()
    flash(f"Added {qty} to {item}. Now {stock_qty(item)} in stock.", "ok")
    return redirect(url_for("admin") + "#stock")


@app.post("/admin/complete/<int:cid>")
@role_required("admin")
def admin_complete(cid):
    row = db().execute("SELECT * FROM complaints WHERE id=? AND status='pending'", (cid,)).fetchone()
    if not row:
        flash("That repair is not pending.", "error")
        return redirect(url_for("admin"))
    used = []
    for item, qty in zip(request.form.getlist("item"), request.form.getlist("qty")):
        item, qty = clean(item, 80), to_int(qty, 0)
        if not item or qty <= 0:
            continue
        row = stock_row(item)
        have = row["qty"] if row else 0
        name = row["item"] if row else item
        take = min(qty, have)
        if take < qty:
            flash(f"Only {have} of {name} in stock. Recorded {take}.", "warn")
        if take > 0:
            db().execute("UPDATE stock SET qty=qty-? WHERE key=?", (take, name.lower()))
            used.append(f"{name} x{take}")
    try:
        cost = max(0.0, float(request.form.get("cost") or 0))
    except ValueError:
        cost = 0.0
    db().execute("UPDATE complaints SET status='done', cost=?, items_used=?, done_on=?, done_by=? "
                 "WHERE id=?", (cost, "; ".join(used), today().strftime("%d-%m-%Y"),
                                session["name"], cid))
    db().commit()
    flash(f"Repair {cid} marked complete.", "ok")
    return redirect(url_for("admin"))


@app.get("/admin/password")
@role_required("admin")
def set_password():
    if not session.get("must_change"):
        return redirect(url_for("admin") + "#account")
    return render_template("set_password.html")


@app.post("/admin/password")
@role_required("admin")
def admin_password():
    row = db().execute("SELECT * FROM admins WHERE id=?", (session["ident"],)).fetchone()
    forced = bool(session.get("must_change"))
    new, again = request.form.get("new", ""), request.form.get("again", "")
    back = url_for("set_password") if forced else url_for("admin") + "#account"

    if not forced and not check_password_hash(row["pw_hash"], request.form.get("current", "")):
        flash("Current password is wrong.", "error")
    elif len(new) < 8 or new != again:
        flash("New password must be at least 8 characters and typed the same twice.", "error")
    elif check_password_hash(row["pw_hash"], new):
        flash("Choose a password that is different from your current one.", "error")
    else:
        db().execute("UPDATE admins SET pw_hash=?, must_change=0 WHERE id=?",
                     (generate_password_hash(new), row["id"]))
        db().commit()
        session.pop("must_change", None)
        flash("Password saved. Use it the next time you log in.", "ok")
        return redirect(url_for("admin") if forced else back)
    return redirect(back)


if __name__ == "__main__":
    app.run(debug=True)
