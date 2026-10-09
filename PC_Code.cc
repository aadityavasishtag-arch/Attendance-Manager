/*
  Simple PC-side Attendance Manager (C++17)
  Reads attendance events pasted/entered from the Arduino Serial Monitor
  and maintains students.csv and attendance.csv.

  This is a console application, not an automatic serial-port reader.
  Copy a successful Arduino line such as:
      ATTENDANCE,A1B2C3D4,PRESENT,12345
  and paste it into the program's event prompt.

  Compile:
    g++ attendance_manager.cpp -std=c++17 -o attendance_manager
  Windows MinGW:
    g++ attendance_manager.cpp -std=c++17 -o attendance_manager.exe
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <ctime>
#include <algorithm>

using namespace std;

struct Student {
    string uid;
    string id;
    string name;
    string department;
    string semester;
};

const string STUDENT_FILE = "students.csv";
const string ATTENDANCE_FILE = "attendance.csv";

string trim(const string& s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

string todayDate() {
    time_t now = time(nullptr);
    tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    ostringstream out;
    out << put_time(&localTime, "%Y-%m-%d");
    return out.str();
}

string currentTime() {
    time_t now = time(nullptr);
    tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    ostringstream out;
    out << put_time(&localTime, "%H:%M:%S");
    return out.str();
}

vector<string> splitCSV(const string& line) {
    // Basic CSV parser for fields without embedded commas or quoted values.
    vector<string> fields;
    string field;
    stringstream ss(line);
    while (getline(ss, field, ',')) fields.push_back(trim(field));
    return fields;
}

vector<Student> loadStudents() {
    vector<Student> students;
    ifstream file(STUDENT_FILE);
    string line;
    if (!file) {
        ofstream create(STUDENT_FILE);
        create << "UID,StudentID,Name,Department,Semester\n";
        return students;
    }
    getline(file, line); // header
    while (getline(file, line)) {
        if (trim(line).empty()) continue;
        auto f = splitCSV(line);
        if (f.size() >= 5) {
            students.push_back({f[0], f[1], f[2], f[3], f[4]});
        }
    }
    return students;
}

void ensureAttendanceFile() {
    ifstream in(ATTENDANCE_FILE);
    if (!in) {
        ofstream out(ATTENDANCE_FILE);
        out << "Date,Time,UID,StudentID,Name,Status\n";
    }
}

void saveStudent(const Student& s) {
    ofstream file(STUDENT_FILE, ios::app);
    file << s.uid << ',' << s.id << ',' << s.name << ','
         << s.department << ',' << s.semester << '\n';
}

int findStudentByUID(const vector<Student>& students, const string& uid) {
    for (int i = 0; i < (int)students.size(); i++) {
        if (students[i].uid == uid) return i;
    }
    return -1;
}

bool alreadyMarkedToday(const string& uid) {
    ifstream file(ATTENDANCE_FILE);
    string line;
    getline(file, line);
    const string date = todayDate();
    while (getline(file, line)) {
        auto f = splitCSV(line);
        if (f.size() >= 6 && f[0] == date && f[2] == uid && f[5] == "PRESENT")
            return true;
    }
    return false;
}

void addStudent() {
    auto students = loadStudents();
    Student s;
    cout << "Enter RFID UID (uppercase, no spaces): ";
    getline(cin, s.uid);
    s.uid = trim(s.uid);
    transform(s.uid.begin(), s.uid.end(), s.uid.begin(), ::toupper);
    if (s.uid.empty() || findStudentByUID(students, s.uid) >= 0) {
        cout << "Invalid or already registered UID.\n";
        return;
    }
    cout << "Enter student ID: "; getline(cin, s.id);
    cout << "Enter student name (do not use commas): "; getline(cin, s.name);
    cout << "Enter department: "; getline(cin, s.department);
    cout << "Enter semester: "; getline(cin, s.semester);
    if (trim(s.id).empty() || trim(s.name).empty()) {
        cout << "Student ID and name are required. Student not saved.\n";
        return;
    }
    saveStudent(s);
    cout << "Student saved. Add the same UID/name to the Arduino students[] list too.\n";
}

void listStudents() {
    auto students = loadStudents();
    if (students.empty()) {
        cout << "No students found. Add a student first.\n";
        return;
    }
    cout << left << setw(14) << "UID" << setw(14) << "Student ID"
         << setw(24) << "Name" << setw(16) << "Department" << "Semester\n";
    cout << string(82, '-') << '\n';
    for (const auto& s : students) {
        cout << left << setw(14) << s.uid << setw(14) << s.id
             << setw(24) << s.name.substr(0, 22)
             << setw(16) << s.department.substr(0, 14) << s.semester << '\n';
    }
}

void recordEvent() {
    ensureAttendanceFile();
    auto students = loadStudents();
    cout << "Paste Arduino line (ATTENDANCE,UID,PRESENT,...) or enter UID: ";
    string input;
    getline(cin, input);
    input = trim(input);

    string uid;
    string status = "PRESENT";
    if (input.rfind("ATTENDANCE,", 0) == 0) {
        auto fields = splitCSV(input);
        if (fields.size() < 3) {
            cout << "Invalid attendance line.\n";
            return;
        }
        uid = fields[1];
        status = fields[2];
    } else {
        uid = input;
    }
    transform(uid.begin(), uid.end(), uid.begin(), ::toupper);

    int index = findStudentByUID(students, uid);
    if (index < 0) {
        cout << "UID not registered in students.csv.\n";
        return;
    }
    if (status != "PRESENT") {
        cout << "Arduino status is not PRESENT; record not saved.\n";
        return;
    }
    if (alreadyMarkedToday(uid)) {
        cout << "Attendance already recorded for this student today.\n";
        return;
    }

    ofstream file(ATTENDANCE_FILE, ios::app);
    file << todayDate() << ',' << currentTime() << ',' << uid << ','
         << students[index].id << ',' << students[index].name << ",PRESENT\n";
    cout << "Attendance recorded for " << students[index].name
         << " on " << todayDate() << " at " << currentTime() << ".\n";
}

void showReport() {
    auto students = loadStudents();
    ensureAttendanceFile();

    if (students.empty()) {
        cout << "No students registered.\n";
        return;
    }

    cout << "Attendance report date (YYYY-MM-DD, blank for today): ";
    string date;
    getline(cin, date);
    date = trim(date);
    if (date.empty()) date = todayDate();

    vector<string> presentUIDs;
    ifstream file(ATTENDANCE_FILE);
    string line;
    getline(file, line);
    while (getline(file, line)) {
        auto f = splitCSV(line);
        if (f.size() >= 6 && f[0] == date && f[5] == "PRESENT")
            presentUIDs.push_back(f[2]);
    }

    cout << "\nReport for " << date << "\n";
    cout << left << setw(14) << "Student ID" << setw(26) << "Name" << "Status\n";
    cout << string(55, '-') << '\n';
    int presentCount = 0;
    for (const auto& s : students) {
        bool present = find(presentUIDs.begin(), presentUIDs.end(), s.uid) != presentUIDs.end();
        if (present) presentCount++;
        cout << left << setw(14) << s.id << setw(26) << s.name.substr(0, 24)
             << (present ? "Present" : "Absent") << '\n';
    }
    cout << "\nPresent: " << presentCount
         << " | Absent: " << (int)students.size() - presentCount
         << " | Total registered: " << students.size() << '\n';
}

int main() {
    ensureAttendanceFile();
    // Create students.csv header if missing.
    loadStudents();

    while (true) {
        cout << "\n===== RFID ATTENDANCE MANAGER =====\n"
             << "1. Add student\n"
             << "2. List students\n"
             << "3. Record attendance from RFID event / UID\n"
             << "4. View daily attendance report\n"
             << "5. Exit\n"
             << "Choose option: ";
        string choice;
        getline(cin, choice);

        if (choice == "1") addStudent();
        else if (choice == "2") listStudents();
        else if (choice == "3") recordEvent();
        else if (choice == "4") showReport();
        else if (choice == "5") {
            cout << "Exiting Attendance Manager.\n";
            break;
        } else {
            cout << "Invalid option. Choose 1-5.\n";
        }
    }
    return 0;
}
