# Maintenance Management System

A C++ command-line app for hostel maintenance: students raise complaints, SRD members request items, and admins manage stock and complete repairs. All data is stored in CSV files in `data/`.

## Run on GitHub (Codespaces)

1. Push this folder to a GitHub repo.
2. Click **Code → Codespaces → Create codespace on main**.
3. In the terminal:

```bash
make run
```

The workflow in `.github/workflows/build.yml` also builds the program and runs `make test` on every push.

## Run locally

Needs `g++` (C++17) and `make`. Run from the repository root so `data/` is found.

```bash
make        # build
make run    # build + run
make test   # end-to-end smoke tests
```

To use a different data folder: `MAINT_DATA_DIR=/path/to/data ./maintenance`

## Logins

| User    | How to log in                                        |
|---------|------------------------------------------------------|
| Student | any registration number, password `student123`       |
| SRD     | just enter a name                                    |
| Member  | ID and password from `data/admins.csv` (e.g. 101)    |

Dates are entered as `DD-MM-YYYY` (or type `today`). The day of the week is worked out automatically.

## Data files (`data/`)

| File               | Columns                                           |
|--------------------|---------------------------------------------------|
| `admins.csv`       | `name,id,pass`                                    |
| `duties.csv`       | `duty_member,member2,member3,day,date` (date is `MM/DD/YY`); only `duty_member` is used |
| `stock.csv`        | `item,qty`                                        |
| `students.csv`     | `regno,name` (optional; unknown numbers log in as "Guest Student") |
| `complaints.csv`   | `name,type,id,cost,date,day,duty,room` (pending)  |
| `records.csv`      | same columns (completed repairs)                  |
| `srd_records.csv`  | `name,date,day,item`                              |

Every file has a header row. Item names and people names are matched ignoring case and extra spaces. `data/legacy/` holds your original `SRD.csv` and old `srd_records.csv`, which the program doesn't use.

## Security note

`admins.csv` stores passwords in plain text and the student password is in the source code. Don't push real passwords to a **public** repository; use a private repo or change them first.
