# Maintenance Management System (web)

A shared website for hostel maintenance. Everyone uses the same live data.

- **Students** log in, report a problem and follow its status.
- **SRD members** request items (only items that are in stock).
- **Admins** see pending repairs, complete them (stock is deducted), manage stock and see SRD requests.

Built with Flask and SQLite. Complaint IDs start at 100 and never repeat.

## Run it on your computer or in Codespaces

```bash
pip install -r requirements.txt
ADMIN_PASSWORD='omsrisairam' python seed.py       # creates maintenance.db
flask --app app run                               # opens on port 5000
```

In Codespaces, click **Open in Browser** on the port 5000 pop-up. Run the tests with `python -m unittest discover -s tests -v`.

## Put it online for everyone (free, PythonAnywhere)

PythonAnywhere keeps its files between restarts, which the SQLite database needs.

1. Push this folder to GitHub, then sign up at pythonanywhere.com. Your username becomes the web address (`username.pythonanywhere.com`).
2. Open **Consoles → Bash** and run (change the repo address and folder to match yours):

```bash
git clone https://github.com/YOUR-NAME/YOUR-REPO.git
cd YOUR-REPO/maintenance-web
python3 -m venv ~/venv && source ~/venv/bin/activate
pip install -r requirements.txt
ADMIN_PASSWORD='omsrisairam' python seed.py
```

3. Open the **Web** tab → **Add a new web app** → **Manual configuration** → pick a recent Python 3. Then set:
   - **Source code** and **Working directory**: `/home/USERNAME/YOUR-REPO/maintenance-web`
   - **Virtualenv**: `/home/USERNAME/venv`
4. Click the **WSGI configuration file** link, delete its contents and paste:

```python
import os, sys
path = '/home/USERNAME/YOUR-REPO/maintenance-web'
if path not in sys.path:
    sys.path.insert(0, path)

os.environ['SECRET_KEY'] = 'paste-a-long-random-string'
os.environ['STUDENT_PASSWORD'] = 'choose-a-student-password'
os.environ['SRD_CODE'] = 'choose-an-srd-code'

from app import app as application
```

5. Click **Reload**. Share `https://USERNAME.pythonanywhere.com`.

Free apps need a click on the Web tab every few months to keep running. To update the site later: `git pull` in a Bash console, then Reload.

## Logins

| Who | Details |
|-----|---------|
| Student | registration number + the student password (`STUDENT_PASSWORD`, default `student123`) |
| SRD | their name + the SRD code (`SRD_CODE`, default `srd123`) |
| Admin | admin ID from `data/admins.csv` + password. The starting password is whatever you set as `ADMIN_PASSWORD` when running `seed.py` (e.g. `omsrisairam`). **On first login each admin is taken straight to a page to choose their own password** and can't use anything else until they do. They can change it again later on the Admin page. |

Until an admin has logged in and chosen a password, anyone who knows the starting password could log in as that admin, so ask everyone to log in soon after launch.

**Change the two defaults before going live.** Set `SECRET_KEY` too; sessions are not safe without it.

## Data

- `data/*.csv` are loaded once by `seed.py`; after that the database is the source of truth.
- Add students by editing `data/students.csv` (`regno,name`) and running `seed.py` again (existing rows are kept). Students not listed can still log in as "Guest Student".
- If you seeded an earlier version, delete `maintenance.db` and run `seed.py` again so the first-login rule applies.
- Back up `maintenance.db` now and then (download it from the PythonAnywhere **Files** tab). It is not stored in git.
- Dates use the `APP_TZ` time zone (default `Asia/Kolkata`).
