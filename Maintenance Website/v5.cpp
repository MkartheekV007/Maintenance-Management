/*
==========================================================
        MAINTENANCE MANAGEMENT SYSTEM (REPAIRED)
==========================================================
*/

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <cctype>
#include <algorithm>

using namespace std;

// ================= GLOBAL HELPERS =================

// Removes hidden Windows carriage returns (\r)
void stripCR(string& str) {
    if (!str.empty() && str.back() == '\r') {
        str.pop_back();
    }
}

// Prevents CSV injection by converting commas to spaces
string sanitizeCSV(string input) {
    for (char &c : input) {
        if (c == ',') c = ' ';
    }
    return input;
}

// Case-insensitive string matching
string toLower(string s) {
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// Prevents infinite loops if user enters text instead of a number
int getSafeInt(string prompt) {
    int val;
    while (true) {
        cout << prompt;
        if (cin >> val) return val;
        cout << "Invalid input. Please enter a valid number.\n";
        cin.clear();
        cin.ignore(10000, '\n');
    }
}


// ================= FILE OPERATIONS =================
namespace FileOperation {

    string getDutyMember(string date, string day) {
        ifstream file("duties.csv");
        if (!file) return "Unassigned";

        string line, fileDate, fileDay, member;
        getline(file, line); // skip header

        while (getline(file, line)) {
            stringstream ss(line);
            getline(ss, fileDate, ',');
            getline(ss, fileDay, ',');
            getline(ss, member); 
            stripCR(member); // Fix \r on last column

            if (fileDate == date && fileDay == day)
                return member;
        }
        return "Unassigned";
    }

    void saveComplaint(string name, string repairType, int repairId, double cost,
                       string date, string day, string dutyMember, string address) {
        ofstream file("complaints.csv", ios::app);
        file << name << "," << repairType << "," << repairId << "," << cost << ","
             << date << "," << day << "," << dutyMember << "," << address << "\n";
        cout << "Complaint saved successfully!\n";
    }

    void saveRecord(string name, string type, int id, double cost,
                    string date, string day, string duty, string room) {
        ofstream file("records.csv", ios::app);
        file << name << "," << type << "," << id << "," << cost << ","
             << date << "," << day << "," << duty << "," << room << "\n";
    }
}


// ================= STOCK =================
class Stock {
public:
    int checkAvailability(string item) {
        ifstream file("stock.csv");
        if (!file) return 0;

        string line, name, qty;
        getline(file, line); // Skip header

        while (getline(file, line)) {
            stringstream ss(line);
            getline(ss, name, ',');
            getline(ss, qty);
            stripCR(qty);

            if (toLower(name) == toLower(item)) {
                try { return stoi(qty); } catch (...) {}
            }
        }
        return 0;
    }

    void updateStock(string item, int take) {
        ifstream file("stock.csv");
        vector<string> lines;
        string line;
        bool updated = false;

        if (file) {
            getline(file, line);
            lines.push_back(line); // Keep header
            while (getline(file, line)) {
                stringstream ss(line);
                string name, qty;
                getline(ss, name, ',');
                getline(ss, qty);
                stripCR(qty);

                int q = 0;
                try { q = stoi(qty); } catch (...) {}

                if (toLower(name) == toLower(item)) {
                    q -= take;
                    if (q < 0) q = 0;
                    updated = true;
                }
                lines.push_back(name + "," + to_string(q));
            }
            file.close();
        }

        ofstream out("stock.csv");
        for (const string& x : lines) out << x << "\n";
    }
};


// ================= REPAIRS =================
class Repairs {
protected:
    int repairId;
    static int nextId;

    static int getHighestId() {
        int maxId = 99;
        ifstream file("complaints.csv");
        if (!file) return maxId;
        string line;
        getline(file, line); // skip header
        while (getline(file, line)) {
            stringstream ss(line);
            string temp, idStr;
            getline(ss, temp, ','); getline(ss, temp, ','); getline(ss, idStr, ',');
            try {
                int id = stoi(idStr);
                if (id > maxId) maxId = id;
            } catch (...) {}
        }
        return maxId;
    }

public:
    Repairs() {
        static bool init = false;
        if (!init) { nextId = getHighestId() + 1; init = true; }
        repairId = nextId++;
    }
    virtual void diagnose(string room) = 0;
    virtual string getTypeName() = 0;
    int getRepairId() { return repairId; }
};

int Repairs::nextId = 100;

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
    int id;
    string name;
public:
    virtual void inputDetails() = 0;
};


// ================= MEMBER =================
class Member : public Person {
public:
    bool login() {
        cout << "\n===== ADMIN LOGIN =====\n";
        int uid = getSafeInt("Enter ID: ");
        string pass;
        cout << "Enter Password: ";
        cin >> pass;

        ifstream file("admins.csv");
        if (!file) {
            cout << "admins.csv file not found!\n";
            return false;
        }

        string line;
        getline(file, line); // Skip header

        while (getline(file, line)) {
            stringstream ss(line);
            string fileID, fileName, filePassword;

            getline(ss, fileID, ',');
            getline(ss, fileName, ',');
            getline(ss, filePassword); 
            stripCR(filePassword);

            try {
                if (!fileID.empty() && stoi(fileID) == uid && filePassword == pass) {
                    id = uid;
                    name = fileName;
                    cout << "\nLogin Successful! Welcome " << name << "!\n";
                    return true;
                }
            } catch (...) {}
        }
        cout << "Invalid Login!\n";
        return false;
    }

    void inputDetails() {
        cout << "Logged in as Admin: " << name << "\n";
    }

    void updatestock() {
        cout << "\n===== UPDATE STOCK =====\n";
        string item;
        cout << "Enter Item: ";
        cin >> ws; getline(cin, item);
        item = sanitizeCSV(item);
        
        int qty = getSafeInt("Enter Quantity: ");

        ofstream file("stock.csv", ios::app);
        file << item << "," << qty << "\n";
        cout << "Stock added successfully!\n";
    }

    void viewMyComplaints() {
        ifstream file("complaints.csv");
        if (!file) {
            cout << "No complaints file found!\n";
            return;
        }
        string line;
        cout << "\n===== MY ASSIGNED COMPLAINTS =====\n";
        getline(file, line); // Skip header
        bool found = false;

        while (getline(file, line)) {
            stringstream ss(line);
            string n, t, i, c, date, day, m, r;
            getline(ss, n, ','); getline(ss, t, ','); getline(ss, i, ',');
            getline(ss, c, ','); getline(ss, date, ','); getline(ss, day, ',');
            getline(ss, m, ','); getline(ss, r);
            stripCR(m); stripCR(r);

            if (m == this->name) {
                found = true;
                cout << "Repair ID: " << i << " | Type: " << t << " | Room: " << r << "\n";
            }
        }
        if (!found) cout << "No complaints assigned to you.\n";
    }

    void pendeningcomplaint() {
        ifstream file("complaints.csv");
        if (!file) return;
        string line;
        cout << "\n===== ALL PENDING COMPLAINTS =====\n";
        getline(file, line); // header

        while (getline(file, line)) {
            stringstream ss(line);
            string n, t, i, c, date, day, m, r;
            getline(ss, n, ','); getline(ss, t, ','); getline(ss, i, ',');
            getline(ss, c, ','); getline(ss, date, ','); getline(ss, day, ',');
            getline(ss, m, ','); getline(ss, r);
            stripCR(r);
            cout << "ID: " << i << " | Assigned: " << m << " | Type: " << t << " | Room: " << r << "\n";
        }
    }

    void markcomplete() {
        ifstream file("complaints.csv");
        if (!file) { cout << "No complaints file found!\n"; return; }

        vector<string> rec;
        string line;
        int repId = getSafeInt("\nEnter Repair ID to Complete: ");
        bool found = false;

        getline(file, line);
        rec.push_back(line); // keep header

        while (getline(file, line)) {
            stringstream ss(line);
            string n, t, iStr, c, date, day, m, r;
            getline(ss, n, ','); getline(ss, t, ','); getline(ss, iStr, ',');
            getline(ss, c, ','); getline(ss, date, ','); getline(ss, day, ',');
            getline(ss, m, ','); getline(ss, r);
            stripCR(r); stripCR(m);

            bool isCompleted = false;

            try {
                if (!iStr.empty() && stoi(iStr) == repId) {
                    if (m != this->name) {
                        cout << "Warning: This ID is assigned to " << m << ". Proceeding anyway.\n";
                    }
                    found = true;
                    Stock stock;
                    char more = 'y';

                    while (tolower(more) == 'y') {
                        string item;
                        cout << "\nEnter Item Used (or 'none'): ";
                        cin >> ws; getline(cin, item);
                        item = sanitizeCSV(item);

                        if (toLower(item) != "none") {
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
                        cin >> more;
                    }

                    double cost = 0;
                    try { cost = stod(c); } catch (...) {}
                    FileOperation::saveRecord(n, t, repId, cost, date, day, m, r);
                    cout << "\nRepair Completed Successfully!\n";
                    isCompleted = true;
                }
            } catch (...) {}

            if (!isCompleted) rec.push_back(line);
        }
        file.close();

        ofstream out("complaints.csv");
        for (const string &x : rec) out << x << "\n";
        if (!found) cout << "\nRepair ID not found!\n";
    }
};


// ================= SRD =================
class SRD : public Person {
public:
    void inputDetails() {
        cout << "Enter SRD Name: ";
        cin >> ws; getline(cin, name);
        name = sanitizeCSV(name);
    }

    void requestItem() {
        string date, day, item;
        cout << "Enter Date (DD-MM-YYYY): "; cin >> date;
        cout << "Enter Day: "; cin >> day;
        cout << "Enter Item: "; cin >> ws; getline(cin, item);
        item = sanitizeCSV(item);

        Stock stock;
        int qty = stock.checkAvailability(item);

        if (qty > 0) {
            cout << "\nItem: " << item << " is Available (Qty: " << qty << ")\n";
            ofstream file("srd_records.csv", ios::app);
            file << name << "," << date << "," << day << "," << item << "\n";
            cout << "Request saved successfully!\n";
        } else {
            cout << "\nItem is not available in stock. Please contact Maintenance.\n";
        }
    }

    void viewPreviousItems() {
        ifstream file("srd_records.csv");
        if (!file) { cout << "No previous records found!\n"; return; }
        string line;
        cout << "\n===== PREVIOUS REQUESTS =====\n";
        while (getline(file, line)) cout << line << "\n";
    }
};


// ================= NON MEMBER =================
class NonMember : public Person {
public:
    void inputDetails() {
        cout << "\n===== STUDENT LOGIN =====\n";
        int regNo = getSafeInt("Enter Registration No: ");
        string password;
        cout << "Enter Password: "; cin >> password;

        if (password != "student123") {
            cout << "Invalid Password!\n";
            exit(0);
        }

        ifstream file("students.csv");
        if (!file) {
            cout << "students.csv not found! Creating default login...\n";
            id = regNo; name = "Student";
            return;
        }

        string line;
        getline(file, line); // Skip header
        bool found = false;

        while (getline(file, line)) {
            stringstream ss(line);
            string fileRegNo, fileName;
            getline(ss, fileRegNo, ',');
            getline(ss, fileName);
            stripCR(fileName);

            try {
                if (!fileRegNo.empty() && stoi(fileRegNo) == regNo) {
                    id = regNo; name = fileName;
                    found = true; break;
                }
            } catch (...) {}
        }
        file.close();

        if (!found) {
            cout << "Registration Number not found in DB! Proceeding as Guest.\n";
            id = regNo; name = "Guest Student";
        }
        cout << "\nLogin Successful! Welcome " << name << "!\n";
    }

    void raiseComplaint(string date, string day) {
        string dutyMember = FileOperation::getDutyMember(date, day);
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
        if (choice == 1) { // Plumbing specific
            int side = getSafeInt("Side (1:A14, 2:A05, 3:B12, 4:B1): ");
            int prob = getSafeInt("Problem (1:Tap, 2:Bath, 3:Washroom): ");
            string num; cout << "Enter Specific Number/ID: "; cin >> num;
            room = "Plumbing Zone " + to_string(side) + "-" + to_string(prob) + " #" + num;
        } else {
            cout << "Enter Room Details: ";
            cin >> ws; getline(cin, room);
            room = sanitizeCSV(room);
        }

        FileOperation::saveComplaint(name, repair->getTypeName(), repair->getRepairId(), 0, date, day, dutyMember, room);
        cout << "\nYour Repair ID is: " << repair->getRepairId() << "\n";
        delete repair;
    }

    void checkRepairUpdates() {
        int checkId = getSafeInt("\nEnter your Repair ID to check status: ");
        bool found = false;

        // Check Complaints (Pending)
        ifstream compFile("complaints.csv");
        string line;
        if (compFile) {
            getline(compFile, line);
            while (getline(compFile, line)) {
                stringstream ss(line);
                string n, t, iStr;
                getline(ss, n, ','); getline(ss, t, ','); getline(ss, iStr, ',');
                if (!iStr.empty() && stoi(iStr) == checkId) {
                    cout << "Status: PENDING. (Assigned to Maintenance)\n";
                    found = true; break;
                }
            }
            compFile.close();
        }

        // Check Records (Completed)
        if (!found) {
            ifstream recFile("records.csv");
            if (recFile) {
                getline(recFile, line);
                while (getline(recFile, line)) {
                    stringstream ss(line);
                    string n, t, iStr;
                    getline(ss, n, ','); getline(ss, t, ','); getline(ss, iStr, ',');
                    if (!iStr.empty() && stoi(iStr) == checkId) {
                        cout << "Status: RESOLVED / COMPLETED.\n";
                        found = true; break;
                    }
                }
                recFile.close();
            }
        }
        if (!found) cout << "Repair ID not found in system.\n";
    }
};

// ================= MAIN =================
int main() {
    int type = getSafeInt("1 Student\n2 SRD\n3 Member\nEnter User Type: ");

    if (type == 1) {
        NonMember n;
        n.inputDetails();

        while (true) {
            int ch = getSafeInt("\n1 Raise Complaint\n2 Check Repair Update\n3 Exit\nEnter choice: ");
            if (ch == 1) {
                string date, day;
                cout << "Enter Date (DD-MM-YYYY): "; cin >> date;
                cout << "Enter Day: "; cin >> day;
                n.raiseComplaint(date, day);
            }
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
            else if (ch == 3) m.pendeningcomplaint();
            else if (ch == 4) m.markcomplete();
            else break;
        }
    } else {
        cout << "Invalid User Type.\n";
    }

    return 0;
}