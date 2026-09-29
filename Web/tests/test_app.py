import os, sys, tempfile, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import app as appmod
from seed import init_db


class Base(unittest.TestCase):
    def setUp(self):
        fd, self.path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        init_db(self.path, "testpass123")
        appmod.app.config.update(DB=self.path, TESTING=True)
        self.c = appmod.app.test_client()

    def tearDown(self):
        os.unlink(self.path)

    def post(self, url, **data):
        with self.c.session_transaction() as s:
            s["csrf"] = "t"
        return self.c.post(url, data={"csrf": "t", **data}, follow_redirects=True)

    def login(self, role, ident, password):
        return self.post("/login", role=role, ident=ident, password=password)

    def admin(self, ident="108"):
        """Log in as an admin, choosing a personal password on the first login."""
        self.c = appmod.app.test_client()
        mine = "Personal#" + ident
        r = self.login("admin", ident, mine)
        if b"don&#39;t match" in r.data:
            self.login("admin", ident, "testpass123")
            r = self.post("/admin/password", new=mine, again=mine)
        return r


class Tests(Base):
    def test_login_rules(self):
        self.assertIn(b"don&#39;t match", self.login("admin", "101", "wrong").data)
        self.assertIn(b"Pending repairs", self.admin("101").data)
        self.assertEqual(self.c.get("/admin").status_code, 200)
        c2 = appmod.app.test_client()
        self.assertEqual(c2.get("/admin").status_code, 302)   # not logged in
        self.assertEqual(c2.post("/login", data={}).status_code, 400)  # no CSRF token

    def test_student_complaints_get_unique_ids_and_duty(self):
        self.login("student", "123", "student123")
        r = self.post("/student/complaint", date="2026-09-29", type="Electric", room="A9")
        self.assertIn(b">100<", r.data)
        self.assertIn(b"Preetam", r.data)          # duty member for 29 Sep 2026
        r = self.post("/student/complaint", date="2026-09-29", type="Plumbing", room="B1 tap")
        self.assertIn(b">101<", r.data)
        other = appmod.app.test_client()
        self.c = other
        self.login("student", "456", "student123")
        self.assertNotIn(b"A9", self.c.get("/student").data)   # sees only own

    def test_bad_input_rejected(self):
        self.login("student", "123", "student123")
        r = self.post("/student/complaint", date="31-02-2026", type="Electric", room="A9")
        self.assertIn(b"Choose a date", r.data)

    def test_srd_stock_flow_and_admin_view(self):
        self.login("srd", "tester", "srd123")
        self.assertIn(b"not in stock", self.post("/srd/request", date="2026-09-29", item="Mic").data)
        self.admin()
        self.post("/admin/stock", item="Mic", qty="5")
        self.post("/admin/stock", item="mic ", qty="5")     # merges, not duplicates
        self.assertIn(b"10", self.c.get("/admin").data)
        self.c = appmod.app.test_client()
        self.login("srd", "tester", "srd123")
        self.assertIn(b"is available", self.post("/srd/request", date="2026-09-29", item="MIC").data)
        self.admin()
        page = self.c.get("/admin").data
        self.assertIn(b"tester", page)
        self.assertIn(b"MIC", page)

    def test_complete_repair_uses_stock_and_updates_student_view(self):
        self.login("student", "123", "student123")
        self.post("/student/complaint", date="2026-09-29", type="Electric", room="A9")
        self.admin()
        self.post("/admin/stock", item="Bulb", qty="3")
        r = self.post("/admin/complete/100", item="bulb", qty="5")   # more than in stock
        self.assertIn(b"Only 3 of Bulb", r.data)
        self.assertIn(b"Bulb x3", r.data)
        self.assertIn(b"marked complete", r.data)
        row = appmod.sqlite3.connect(self.path).execute("SELECT qty FROM stock WHERE key='bulb'").fetchone()
        self.assertEqual(row[0], 0)
        self.c = appmod.app.test_client()
        self.login("student", "123", "student123")
        self.assertIn(b"Fixed", self.c.get("/student").data)

    def test_first_login_forces_own_password(self):
        r = self.login("admin", "102", "testpass123")
        self.assertIn(b"Choose your own password", r.data)
        # nothing else is reachable until a password is chosen
        for url in ("/admin", "/admin/stock"):
            self.assertIn(b"Choose your own password", self.c.get(url, follow_redirects=True).data)
        self.assertIn(b"at least 8", self.post("/admin/password", new="short", again="short").data)
        self.assertIn(b"at least 8", self.post("/admin/password", new="longenough1", again="different1").data)
        self.assertIn(b"different from your current",
                      self.post("/admin/password", new="testpass123", again="testpass123").data)
        r = self.post("/admin/password", new="MyOwn#pass9", again="MyOwn#pass9")
        self.assertIn(b"Pending repairs", r.data)
        # old starting password no longer works; the new one goes straight in
        self.c = appmod.app.test_client()
        self.assertIn(b"don&#39;t match", self.login("admin", "102", "testpass123").data)
        r = self.login("admin", "102", "MyOwn#pass9")
        self.assertIn(b"Pending repairs", r.data)
        self.assertNotIn(b"Choose your own password", r.data)

    def test_each_admin_has_their_own_password(self):
        self.admin("103")                       # 103 picks a password
        self.c = appmod.app.test_client()       # 104 has not: still forced
        self.assertIn(b"Choose your own password", self.login("admin", "104", "testpass123").data)

    def test_change_password_later(self):
        self.admin("101")
        r = self.post("/admin/password", current="Personal#101", new="newpass456", again="newpass456")
        self.assertIn(b"Password saved", r.data)
        self.assertIn(b"Current password is wrong",
                      self.post("/admin/password", current="nope", new="another789", again="another789").data)
        self.c = appmod.app.test_client()
        self.assertIn(b"Pending repairs", self.login("admin", "101", "newpass456").data)


if __name__ == "__main__":
    unittest.main()
