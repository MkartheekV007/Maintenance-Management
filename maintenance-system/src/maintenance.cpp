/*
==========================================================
        MAINTENANCE MANAGEMENT SYSTEM  (v6)
==========================================================
 Run from the repository root:   make run
 Data files live in ./data  (override with env MAINT_DATA_DIR)
*/

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cctype>
#include <limits>
#include <algorithm>

using namespace std;

// ================= FILE NAMES =================

string dataPath(const string& file) {
    const char* env = getenv("MAINT_DATA_DIR");
    string dir = env ? env : "data";
    if (!dir.empty() && dir.back() != '/') dir += '/';
    return dir + file;
}

const string COMPLAINT_HEADER = "name,type,id,cost,date,day,duty,room";

// ================= STRING / CSV HELPERS =================

string trim(const string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
    return s;
}

// Normalised form used for every comparison: trimmed + lowercase
string norm(const string& s) { return toLower(trim(s)); }

// Keeps user text safe to store in a CSV field
string sanitizeCSV(string s) {
    for (char& c : s)
        if (c == ',' || c == '\r' || c == '\n') c = ' ';
    return trim(s);
}

vector<string> splitCSV(const string& line) {
    vector<string> out;
    string cell;
    stringstream ss(line);
    while (getline(ss, cell, ',')) out.push_back(trim(cell));
    if (!line.empty() && line.back() == ',') out.push_back("");
    return out;
}

bool toInt(const string& s, int& out) {
    try {
        size_t pos = 0;
        out = stoi(trim(s), &pos);
        return true;
    } catch (...) { return false; }
}

// Reads a CSV file, skips the header line and blank lines.
vector<vector<string>> readRows(const string& file) {
    vector<vector<string>> rows;
    ifstream in(dataPath(file));
    if (!in) return rows;
    string line;
    getline(in, line); // header
    while (getline(in, line)) {
        if (trim(line).empty()) continue;
        rows.push_back(splitCSV(line));
    }
    return rows;
}

void writeRows(const string& file, const string& header, const vector<vector<string>>& rows) {
    ofstream out(dataPath(file));
    out << header << "\n";
    for (const auto& r : rows) {
        for (size_t i = 0; i < r.size(); i++) out << (i ? "," : "") << r[i];
        out << "\n";
    }
}

// Creates the file with just a header if it is missing or empty
void ensureFile(const string& file, const string& header) {
    ifstream in(dataPath(file));
    if (in && in.peek() != ifstream::traits_type::eof()) return;
    ofstream out(dataPath(file));
    out << header << "\n";
}

void appendRow(const string& file, const string& header, const string& line) {
    ensureFile(file, header);
    ofstream out(dataPath(file), ios::app);
    out << line << "\n";
}

// ================= INPUT HELPERS =================

int getSafeInt(const string& prompt) {
    int val;
    while (true) {
        cout << prompt;
        if (cin >> val) return val;
        if (cin.eof()) { cout << "\nInput ended. Exiting.\n"; exit(0); }
        cout << "Invalid input. Please enter a valid number.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

string readWord(const string& prompt) {
    string s;
    cout << prompt;
    if (!(cin >> s)) { cout << "\nInput ended. Exiting.\n"; exit(0); }
    return s;
}

string readLine(const string& prompt) {
    string s;
    cout << prompt;
    cin >> ws;
    if (!getline(cin, s)) { cout << "\nInput ended. Exiting.\n"; exit(0); }
    return sanitizeCSV(s);
}

// ================= DATES =================

struct DateInfo {
    string display; // DD-MM-YYYY
    string key;     // YYYY-MM-DD  (used for matching)
    string day;     // Monday ...
};

static bool buildDate(int d, int m, int y, DateInfo& out) {
    if (y < 100) y += 2000;
    tm t = {};
    t.tm_mday = d; t.tm_mon = m - 1; t.tm_year = y - 1900; t.tm_hour = 12;
    if (mktime(&t) == (time_t)-1) return false;
    if (t.tm_mday != d || t.tm_mon != m - 1 || t.tm_year != y - 1900) return false; // e.g. 31-02
    static const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};
    char buf[64];
    snprintf(buf, sizeof buf, "%02d-%02d-%04d", d, m, y);
    out.display = buf;
    snprintf(buf, sizeof buf, "%04d-%02d-%02d", y, m, d);
    out.key = buf;
    out.day = days[t.tm_wday];
    return true;
}

// User input: DD-MM-YYYY (or DD/MM/YYYY), or the word "today"
bool parseUserDate(string s, DateInfo& out) {
    s = trim(s);
    if (toLower(s) == "today") {
        time_t now = time(nullptr);
        tm* t = localtime(&now);
        return buildDate(t->tm_mday, t->tm_mon + 1, t->tm_year + 1900, out);
    }
    replace(s.begin(), s.end(), '/', '-');
    int d, m, y;
    char extra;
    if (sscanf(s.c_str(), "%d-%d-%d%c", &d, &m, &y, &extra) != 3) return false;
    return buildDate(d, m, y, out);
}

// duties.csv stores dates as MM/DD/YY
bool parseDutyDate(const string& s, DateInfo& out) {
    int m, d, y;
    char extra;
    if (sscanf(trim(s).c_str(), "%d/%d/%d%c", &m, &d, &y, &extra) != 3) return false;
    return buildDate(d, m, y, out);
}

DateInfo askDate() {
    DateInfo d;
    while (true) {
        string s = readWord("Enter Date (DD-MM-YYYY, or 'today'): ");
        if (parseUserDate(s, d)) {
            cout << "Day: " << d.day << "\n";
            return d;
        }
        cout << "Invalid date. Example: 29-09-2026\n";
    }
}

// ================= FILE OPERATIONS =================
namespace FileOperation {

    // duties.csv columns: duty_member, member2, member3, day, date(MM/DD/YY)
    string getDutyMember(const DateInfo& date) {
        for (const auto& r : readRows("duties.csv")) {
            if (r.size() < 5) continue;
            DateInfo d;
            if (parseDutyDate(r[4], d) && d.key == date.key)
                return r[0].empty() ? "Unassigned" : r[0];
        }
        return "Unassigned";
    }

    void saveComplaint(string name, string type, int id, double cost,
                       string date, string day, string duty, string room) {
        ostringstream line;
        line << sanitizeCSV(name) << "," << type << "," << id << "," << cost << ","
             << date << "," << day << "," << sanitizeCSV(duty) << "," << sanitizeCSV(room);
        appendRow("complaints.csv", COMPLAINT_HEADER, line.str());
        cout << "Complaint saved successfully!\n";
    }

    void saveRecord(string name, string type, int id, double cost,
                    string date, string day, string duty, string room) {
        ostringstream line;
        line << sanitizeCSV(name) << "," << type << "," << id << "," << cost << ","
             << date << "," << day << "," << sanitizeCSV(duty) << "," << sanitizeCSV(room);
        appendRow("records.csv", COMPLAINT_HEADER, line.str());
    }
}

// ================= STOCK =================
// stock.csv columns: item, qty
class Stock {
    struct Item { string name; int qty; };
    vector<Item> items;

    void load() {
        items.clear();
        for (const auto& r : readRows("stock.csv")) {
            if (r.empty() || r[0].empty()) continue;
            int q = 0;
            if (r.size() > 1) toInt(r[1], q);
            Item* found = nullptr;
            for (auto& it : items) if (norm(it.name) == norm(r[0])) { found = &it; break; }
            if (found) found->qty += q;
            else items.push_back({r[0], q});
        }
    }

    void save() {
        vector<vector<string>> rows;
        for (const auto& it : items) rows.push_back({it.name, to_string(it.qty)});
        writeRows("stock.csv", "item,qty", rows);
    }

public:
    int checkAvailability(const string& item) {
        load();
        for (const auto& it : items) if (norm(it.name) == norm(item)) return it.qty;
        return 0;
    }

    // Subtract used quantity (never below zero)
    void updateStock(const string& item, int take) {
        load();
        for (auto& it : items)
            if (norm(it.name) == norm(item)) {
                it.qty = max(0, it.qty - take);
                break;
            }
        save();
    }

    // Add quantity to an existing item, or create the item
    void addStock(const string& item, int qty) {
        load();
        bool found = false;
        for (auto& it : items)
            if (norm(it.name) == norm(item)) { it.qty += qty; found = true; break; }
        if (!found) items.push_back({trim(item), qty});
        save();
    }
};

// ================= REPAIRS =================
class Repairs {
protected:
    int repairId;

    static int highestId() {
        int maxId = 99; // first ID is 100
        for (const char* f : {"complaints.csv", "records.csv"})
            for (const auto& r : readRows(f)) {
                int id;
                if (r.size() > 2 && toInt(r[2], id) && id > maxId) maxId = id;
            }
        return maxId;
    }

public:
    Repairs() { repairId = highestId() + 1; }
    virtual ~Repairs() {}
    virtual void diagnose(string room) = 0;
    virtual string getTypeName() = 0;
    int getRepairId() { return repairId; }
};

class Plumbing : public Repairs {
public:
    void diagnose(string room) { cout << "Plumbing issue in " << room << "\n"; }
    string getTypeName() { return "Plumbing"; }
};

class Electric : public Repairs {
public:
    void diagnose(string room) { cout << "Electric issue in " << room << "\n"; }
    string getTypeName() { return "Electric"; }
};

class Carpentry : public Repairs {
public:
    void diagnose(string room) { cout << "Carpentry issue in " << room << "\n"; }
    string getTypeName() { return "Carpentry"; }
};

// ================= PERSON =================
class Person {
protected:
    int id = 0;
    string name;
public:
    virtual ~Person() {}
    virtual void inputDetails() = 0;
};

// ================= MEMBER (ADMIN) =================
class Member : public Person {
public:
    // admins.csv columns: name, id, pass
    bool login() {
        cout << "\n===== ADMIN LOGIN =====\n";
        int uid = getSafeInt("Enter ID: ");
        string pass = readWord("Enter Password: ");

        auto rows = readRows("admins.csv");
        if (rows.empty()) {
            cout << "admins.csv is missing or empty!\n";
            return false;
        }
        for (const auto& r : rows) {
            int fileId;
            if (r.size() >= 3 && toInt(r[1], fileId) && fileId == uid && r[2] == pass) {
                id = uid;
                name = r[0];
                cout << "\nLogin Successful! Welcome " << name << "!\n";
                return true;
            }
        }
        cout << "Invalid Login!\n";
        return false;
    }

    void inputDetails() { cout << "Logged in as Admin: " << name << "\n"; }

    void updatestock() {
        cout << "\n===== UPDATE STOCK =====\n";
        string item = readLine("Enter Item: ");
        if (item.empty()) { cout << "Item name cannot be empty.\n"; return; }
        int qty = getSafeInt("Enter Quantity to add: ");
        if (qty < 0) { cout << "Quantity cannot be negative.\n"; return; }
        Stock stock;
        stock.addStock(item, qty);
        cout << "Stock updated! " << item << " now has " << stock.checkAvailability(item) << ".\n";
    }

    void viewMyComplaints() {
        cout << "\n===== MY ASSIGNED COMPLAINTS =====\n";
        bool found = false;
        for (const auto& r : readRows("complaints.csv")) {
            if (r.size() < 8) continue;
            if (norm(r[6]) == norm(name)) {
                found = true;
                cout << "Repair ID: " << r[2] << " | Type: " << r[1] << " | Room: " << r[7]
                     << " | Date: " << r[4] << "\n";
            }
        }
        if (!found) cout << "No complaints assigned to you.\n";
    }

    void pendingComplaints() {
        cout << "\n===== ALL PENDING COMPLAINTS =====\n";
        auto rows = readRows("complaints.csv");
        if (rows.empty()) { cout << "No pending complaints.\n"; return; }
        for (const auto& r : rows) {
            if (r.size() < 8) continue;
            cout << "ID: " << r[2] << " | Assigned: " << r[6] << " | Type: " << r[1]
                 << " | Room: " << r[7] << " | Date: " << r[4] << "\n";
        }
    }

    void markcomplete() {
        auto rows = readRows("complaints.csv");
        if (rows.empty()) { cout << "\nNo pending complaints.\n"; return; }

        int repId = getSafeInt("\nEnter Repair ID to Complete: ");
        int index = -1;
        for (size_t i = 0; i < rows.size(); i++) {
            int id;
            if (rows[i].size() >= 8 && toInt(rows[i][2], id) && id == repId) { index = (int)i; break; }
        }
        if (index < 0) { cout << "\nRepair ID not found!\n"; return; }

        const auto r = rows[index];
        if (norm(r[6]) != norm(name))
            cout << "Warning: This ID is assigned to " << r[6] << ". Proceeding anyway.\n";

        Stock stock;
        char more = 'y';
        while (tolower(more) == 'y') {
            string item = readLine("\nEnter Item Used (or 'none'): ");
            if (toLower(item) != "none" && !item.empty()) {
                int qty = getSafeInt("Enter Quantity Used: ");
                int available = stock.checkAvailability(item);
                if (qty > available) {
                    cout << "WARNING: Only " << available << " in stock! Recording max available.\n";
                    qty = available;
                }
                if (qty > 0) {
                    stock.updateStock(item, qty);
                    cout << "Stock updated.\n";
                }
            }
            cout << "Enter another item? (y/n): ";
            if (!(cin >> more)) break;
        }

        double cost = 0;
        try { cost = stod(r[3]); } catch (...) {}
        FileOperation::saveRecord(r[0], r[1], repId, cost, r[4], r[5], r[6], r[7]);

        rows.erase(rows.begin() + index);
        writeRows("complaints.csv", COMPLAINT_HEADER, rows);
        cout << "\nRepair Completed Successfully!\n";
    }
};

// ================= SRD =================
class SRD : public Person {
public:
    void inputDetails() {
        name = readLine("Enter SRD Name: ");
        if (name.empty()) name = "SRD";
    }

    void requestItem() {
        DateInfo date = askDate();
        string item = readLine("Enter Item: ");

        Stock stock;
        int qty = stock.checkAvailability(item);

        if (qty > 0) {
            cout << "\nItem: " << item << " is Available (Qty: " << qty << ")\n";
            appendRow("srd_records.csv", "name,date,day,item",
                      name + "," + date.display + "," + date.day + "," + item);
            cout << "Request saved successfully!\n";
        } else {
            cout << "\nItem is not available in stock. Please contact Maintenance.\n";
        }
    }

    void viewPreviousItems() {
        cout << "\n===== PREVIOUS REQUESTS =====\n";
        auto rows = readRows("srd_records.csv");
        if (rows.empty()) { cout << "No previous records found!\n"; return; }
        for (const auto& r : rows) {
            for (size_t i = 0; i < r.size(); i++) cout << (i ? " | " : "") << r[i];
            cout << "\n";
        }
    }
};

// ================= NON MEMBER (STUDENT) =================
class NonMember : public Person {
    bool loggedIn = false;
public:
    static const string STUDENT_PASSWORD;

    bool isLoggedIn() const { return loggedIn; }

    // students.csv columns: regno, name
    void inputDetails() {
        cout << "\n===== STUDENT LOGIN =====\n";
        int regNo = getSafeInt("Enter Registration No: ");
        string password = readWord("Enter Password: ");

        if (password != STUDENT_PASSWORD) {
            cout << "Invalid Password!\n";
            return;
        }

        id = regNo;
        name = "Guest Student";
        for (const auto& r : readRows("students.csv")) {
            int fileReg;
            if (r.size() >= 2 && toInt(r[0], fileReg) && fileReg == regNo) { name = r[1]; break; }
        }
        loggedIn = true;
        cout << "\nLogin Successful! Welcome " << name << "!\n";
    }

    void raiseComplaint() {
        DateInfo date = askDate();
        string dutyMember = FileOperation::getDutyMember(date);
        cout << "\nAssigned Duty Member: " << dutyMember << "\n";

        int choice = 0;
        while (true) {
            cout << "\n1 Plumbing\n2 Electric\n3 Carpentry\n";
            choice = getSafeInt("Enter Choice: ");
            if (choice >= 1 && choice <= 3) break;
            cout << "Invalid option! Pick 1, 2, or 3.\n";
        }

        Repairs* repair = nullptr;
        if (choice == 1) repair = new Plumbing();
        else if (choice == 2) repair = new Electric();
        else repair = new Carpentry();

        string room;
        if (choice == 1) {
            static const char* sides[] = {"A14", "A05", "B12", "B1"};
            static const char* problems[] = {"Tap", "Bath", "Washroom"};
            int side = 0, prob = 0;
            while (side < 1 || side > 4) side = getSafeInt("Side (1:A14, 2:A05, 3:B12, 4:B1): ");
            while (prob < 1 || prob > 3) prob = getSafeInt("Problem (1:Tap, 2:Bath, 3:Washroom): ");
            string num = readLine("Enter Specific Number/ID: ");
            room = string(sides[side - 1]) + " Side " + problems[prob - 1] + " " + num;
        } else {
            room = readLine("Enter Room Details: ");
        }

        repair->diagnose(room);
        FileOperation::saveComplaint(name, repair->getTypeName(), repair->getRepairId(), 0,
                                     date.display, date.day, dutyMember, room);
        cout << "\nYour Repair ID is: " << repair->getRepairId() << "\n";
        delete repair;
    }

    void checkRepairUpdates() {
        int checkId = getSafeInt("\nEnter your Repair ID to check status: ");

        for (const auto& r : readRows("complaints.csv")) {
            int id;
            if (r.size() >= 8 && toInt(r[2], id) && id == checkId) {
                cout << "Status: PENDING. Assigned to " << r[6] << ".\n";
                return;
            }
        }
        for (const auto& r : readRows("records.csv")) {
            int id;
            if (r.size() >= 3 && toInt(r[2], id) && id == checkId) {
                cout << "Status: RESOLVED / COMPLETED.\n";
                return;
            }
        }
        cout << "Repair ID not found in system.\n";
    }
};

const string NonMember::STUDENT_PASSWORD = "student123";

// ================= MAIN =================
int main() {
    ensureFile("complaints.csv", COMPLAINT_HEADER);
    ensureFile("records.csv", COMPLAINT_HEADER);
    ensureFile("srd_records.csv", "name,date,day,item");
    ensureFile("stock.csv", "item,qty");
    ensureFile("students.csv", "regno,name");

    int type = getSafeInt("1 Student\n2 SRD\n3 Member\nEnter User Type: ");

    if (type == 1) {
        NonMember n;
        n.inputDetails();
        if (!n.isLoggedIn()) return 0;

        while (true) {
            int ch = getSafeInt("\n1 Raise Complaint\n2 Check Repair Update\n3 Exit\nEnter choice: ");
            if (ch == 1) n.raiseComplaint();
            else if (ch == 2) n.checkRepairUpdates();
            else break;
        }
    }
    else if (type == 2) {
        SRD s;
        s.inputDetails();
        while (true) {
            int ch = getSafeInt("\n1 Request Item\n2 View Previous\n3 Exit\nEnter choice: ");
            if (ch == 1) s.requestItem();
            else if (ch == 2) s.viewPreviousItems();
            else break;
        }
    }
    else if (type == 3) {
        Member m;
        if (!m.login()) return 0;
        m.inputDetails();
        while (true) {
            int ch = getSafeInt("\n1 Update Stock\n2 My Complaints\n3 All Pending\n4 Complete Repair\n5 Exit\nEnter choice: ");
            if (ch == 1) m.updatestock();
            else if (ch == 2) m.viewMyComplaints();
            else if (ch == 3) m.pendingComplaints();
            else if (ch == 4) m.markcomplete();
            else break;
        }
    }
    else {
        cout << "Invalid User Type.\n";
    }
    return 0;
}
