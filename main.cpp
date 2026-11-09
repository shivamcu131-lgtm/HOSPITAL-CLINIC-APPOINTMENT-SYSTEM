/*
 * ===========================================================================
 *  HOSPITAL / CLINIC APPOINTMENT SYSTEM  (OOPS in C++ - PBL / Mini Project)
 * ===========================================================================
 *  OOP concepts used:
 *    Abstraction ........ abstract base class Person (pure virtual functions)
 *    Inheritance ........ Patient, Doctor derive from Person
 *    Polymorphism ....... virtual display() / serialize(), runtime dispatch
 *    Encapsulation ...... private/protected data with getters/setters
 *    Operator overload .. operator<< and operator< on Appointment
 *    Templates .......... generic Registry<T> container
 *    Exceptions ......... custom exception hierarchy
 *    STL ................ vector, map, priority_queue, algorithm
 *    File handling ...... data saved/loaded from .txt files
 *    Static members ..... Appointment counters, constants
 *
 *  UNIQUE FEATURE  ->  "SMART TRIAGE ENGINE"
 *    1. Every appointment has a severity (Routine / Urgent / Emergency).
 *    2. EMERGENCY patients PRE-EMPT lower-priority appointments. The displaced
 *       patient is AUTOMATICALLY RESCHEDULED to the next free slot.
 *    3. Auto-Book finds the earliest slot for a doctor (no manual slot pick).
 *    4. Wait-Time Estimator tells a patient how long they will wait.
 *    5. Triage Queue shows the order of treatment using a priority queue.
 * ===========================================================================
 */

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

// ----------------------------------------------------------------- constants
const int DAYS = 30;          // booking window (day 1 .. 30)
const int SLOTS = 8;          // 8 slots per day, 30 minutes each
const int SLOT_MINUTES = 30;
const int START_HOUR = 9;     // clinic opens 09:00

// ---------------------------------------------------------------- exceptions
class HospitalException : public runtime_error {
public:
    explicit HospitalException(const string& msg) : runtime_error(msg) {}
};
class NotFoundException : public HospitalException {
public:
    explicit NotFoundException(const string& what)
        : HospitalException(what + " not found.") {}
};
class SlotUnavailableException : public HospitalException {
public:
    explicit SlotUnavailableException(const string& msg) : HospitalException(msg) {}
};
class InvalidInputException : public HospitalException {
public:
    explicit InvalidInputException(const string& msg) : HospitalException(msg) {}
};

// ------------------------------------------------------------------ helpers
enum class Severity { ROUTINE = 1, URGENT = 2, EMERGENCY = 3 };

string severityName(Severity s) {
    switch (s) {
        case Severity::ROUTINE:   return "Routine";
        case Severity::URGENT:    return "Urgent";
        case Severity::EMERGENCY: return "EMERGENCY";
    }
    return "?";
}

string slotTime(int slot) {
    int mins = START_HOUR * 60 + slot * SLOT_MINUTES;
    ostringstream os;
    os << setfill('0') << setw(2) << mins / 60 << ":" << setw(2) << mins % 60;
    return os.str();
}

vector<string> split(const string& line, char sep) {
    vector<string> out;
    string part;
    stringstream ss(line);
    while (getline(ss, part, sep)) out.push_back(part);
    return out;
}

// =============================================================== class Person
class Person {
protected:
    int id;
    string name;
    int age;
    string phone;

public:
    Person(int id, const string& name, int age, const string& phone)
        : id(id), name(name), age(age), phone(phone) {}
    virtual ~Person() {}

    virtual void display() const = 0;            // pure virtual -> abstract
    virtual string serialize() const = 0;
    virtual string role() const = 0;

    int getId() const { return id; }
    const string& getName() const { return name; }
    int getAge() const { return age; }
    const string& getPhone() const { return phone; }
};

// ============================================================== class Patient
class Patient : public Person {
    string ailment;

public:
    Patient(int id, const string& name, int age, const string& phone,
            const string& ailment)
        : Person(id, name, age, phone), ailment(ailment) {}

    const string& getAilment() const { return ailment; }
    string role() const override { return "Patient"; }

    void display() const override {
        cout << left << setw(5) << id << setw(20) << name << setw(6) << age
             << setw(14) << phone << ailment << "\n";
    }
    string serialize() const override {
        return to_string(id) + "|" + name + "|" + to_string(age) + "|" + phone +
               "|" + ailment;
    }
    static Patient deserialize(const string& line) {
        auto f = split(line, '|');
        if (f.size() < 5) throw InvalidInputException("Corrupt patient record");
        return Patient(stoi(f[0]), f[1], stoi(f[2]), f[3], f[4]);
    }
};

// =============================================================== class Doctor
class Doctor : public Person {
    string specialization;
    double fee;

public:
    Doctor(int id, const string& name, int age, const string& phone,
           const string& spec, double fee)
        : Person(id, name, age, phone), specialization(spec), fee(fee) {}

    const string& getSpecialization() const { return specialization; }
    double getFee() const { return fee; }
    string role() const override { return "Doctor"; }

    void display() const override {
        cout << left << setw(5) << id << setw(20) << name << setw(18)
             << specialization << setw(14) << phone << "Rs " << fixed
             << setprecision(0) << fee << "\n";
    }
    string serialize() const override {
        return to_string(id) + "|" + name + "|" + to_string(age) + "|" + phone +
               "|" + specialization + "|" + to_string(fee);
    }
    static Doctor deserialize(const string& line) {
        auto f = split(line, '|');
        if (f.size() < 6) throw InvalidInputException("Corrupt doctor record");
        return Doctor(stoi(f[0]), f[1], stoi(f[2]), f[3], f[4], stod(f[5]));
    }
};

// ========================================================== class Appointment
class Appointment {
    static int totalCreated;          // static member: counts objects created
    int id, patientId, doctorId, day, slot;
    Severity severity;
    bool active;
    int rescheduledCount;

public:
    Appointment(int id, int pid, int did, int day, int slot, Severity sev,
                bool active = true, int resched = 0)
        : id(id), patientId(pid), doctorId(did), day(day), slot(slot),
          severity(sev), active(active), rescheduledCount(resched) {
        ++totalCreated;
    }

    int getId() const { return id; }
    int getPatientId() const { return patientId; }
    int getDoctorId() const { return doctorId; }
    int getDay() const { return day; }
    int getSlot() const { return slot; }
    Severity getSeverity() const { return severity; }
    bool isActive() const { return active; }
    int getRescheduledCount() const { return rescheduledCount; }
    static int getTotalCreated() { return totalCreated; }

    void cancel() { active = false; }
    void moveTo(int newDay, int newSlot) {
        day = newDay;
        slot = newSlot;
        ++rescheduledCount;
    }

    // Priority order for the triage queue: higher severity first, then earlier.
    bool operator<(const Appointment& o) const {
        if (severity != o.severity) return severity < o.severity;
        if (day != o.day) return day > o.day;
        return slot > o.slot;
    }

    string serialize() const {
        return to_string(id) + "|" + to_string(patientId) + "|" +
               to_string(doctorId) + "|" + to_string(day) + "|" +
               to_string(slot) + "|" + to_string(static_cast<int>(severity)) +
               "|" + to_string(active) + "|" + to_string(rescheduledCount);
    }
    static Appointment deserialize(const string& line) {
        auto f = split(line, '|');
        if (f.size() < 8) throw InvalidInputException("Corrupt appointment record");
        return Appointment(stoi(f[0]), stoi(f[1]), stoi(f[2]), stoi(f[3]),
                           stoi(f[4]), static_cast<Severity>(stoi(f[5])),
                           stoi(f[6]) != 0, stoi(f[7]));
    }

    friend ostream& operator<<(ostream& os, const Appointment& a) {
        os << "Appt#" << a.id << "  Day " << a.day << " @ " << slotTime(a.slot)
           << "  Patient#" << a.patientId << "  Doctor#" << a.doctorId << "  ["
           << severityName(a.severity) << "]";
        if (a.rescheduledCount > 0)
            os << "  (rescheduled x" << a.rescheduledCount << ")";
        return os;
    }
};
int Appointment::totalCreated = 0;

// ============================================ template class Registry<T>
// Generic id -> object store, reused for patients, doctors and appointments.
template <typename T>
class Registry {
    map<int, T> items;
    int next = 1;

public:
    int newId() { return next++; }
    void add(const T& item) {
        items.insert({item.getId(), item});
        next = max(next, item.getId() + 1);
    }
    T* find(int id) {
        auto it = items.find(id);
        return it == items.end() ? nullptr : &it->second;
    }
    bool exists(int id) const { return items.count(id) > 0; }
    map<int, T>& all() { return items; }
    const map<int, T>& all() const { return items; }
    size_t size() const { return items.size(); }
};

// ============================================================ class Hospital
class Hospital {
    Registry<Patient> patients;
    Registry<Doctor> doctors;
    Registry<Appointment> appointments;

    // ---- slot helpers -------------------------------------------------------
    Appointment* appointmentAt(int doctorId, int day, int slot) {
        for (auto& kv : appointments.all()) {
            Appointment& a = kv.second;
            if (a.isActive() && a.getDoctorId() == doctorId && a.getDay() == day &&
                a.getSlot() == slot)
                return &a;
        }
        return nullptr;
    }

    // earliest free slot strictly after (day, slot) -> {-1,-1} if none
    pair<int, int> nextFreeAfter(int doctorId, int day, int slot) {
        int s = slot + 1;
        for (int d = day; d <= DAYS; ++d) {
            for (; s < SLOTS; ++s)
                if (!appointmentAt(doctorId, d, s)) return {d, s};
            s = 0;
        }
        return {-1, -1};
    }

    pair<int, int> earliestFree(int doctorId, int fromDay) {
        for (int d = fromDay; d <= DAYS; ++d)
            for (int s = 0; s < SLOTS; ++s)
                if (!appointmentAt(doctorId, d, s)) return {d, s};
        return {-1, -1};
    }

    static void validateSlot(int day, int slot) {
        if (day < 1 || day > DAYS)
            throw InvalidInputException("Day must be between 1 and " + to_string(DAYS));
        if (slot < 0 || slot >= SLOTS)
            throw InvalidInputException("Slot must be between 0 and " + to_string(SLOTS - 1));
    }

    void requireEntities(int pid, int did) {
        if (!patients.exists(pid)) throw NotFoundException("Patient " + to_string(pid));
        if (!doctors.exists(did)) throw NotFoundException("Doctor " + to_string(did));
    }

    // Push a lower-priority appointment to its next free slot.
    void displace(Appointment& victim) {
        auto np = nextFreeAfter(victim.getDoctorId(), victim.getDay(), victim.getSlot());
        if (np.first == -1)
            throw SlotUnavailableException("No free slot left to reschedule the displaced patient.");
        Patient* p = patients.find(victim.getPatientId());
        cout << "  >> TRIAGE: " << (p ? p->getName() : "Patient") << " (was Day "
             << victim.getDay() << " @ " << slotTime(victim.getSlot())
             << ") rescheduled to Day " << np.first << " @ " << slotTime(np.second)
             << "\n";
        victim.moveTo(np.first, np.second);
    }

public:
    // ---- registration -------------------------------------------------------
    int addPatient(const string& name, int age, const string& phone, const string& ailment) {
        if (name.empty()) throw InvalidInputException("Name cannot be empty.");
        if (age <= 0 || age > 120) throw InvalidInputException("Invalid age.");
        int id = patients.newId();
        patients.add(Patient(id, name, age, phone, ailment));
        return id;
    }
    int addDoctor(const string& name, int age, const string& phone, const string& spec, double fee) {
        if (name.empty()) throw InvalidInputException("Name cannot be empty.");
        int id = doctors.newId();
        doctors.add(Doctor(id, name, age, phone, spec, fee));
        return id;
    }

    // ---- booking ------------------------------------------------------------
    // Manual booking. An EMERGENCY may pre-empt a lower-priority appointment.
    int book(int pid, int did, int day, int slot, Severity sev) {
        requireEntities(pid, did);
        validateSlot(day, slot);
        Appointment* existing = appointmentAt(did, day, slot);
        if (existing) {
            if (sev > existing->getSeverity()) {
                displace(*existing);            // frees the slot
            } else {
                throw SlotUnavailableException("Slot already booked by an equal/higher priority patient.");
            }
        }
        int id = appointments.newId();
        appointments.add(Appointment(id, pid, did, day, slot, sev));
        return id;
    }

    // Smart auto-booking: finds the earliest slot. Emergencies pre-empt.
    int autoBook(int pid, int did, Severity sev, int fromDay) {
        requireEntities(pid, did);
        validateSlot(fromDay, 0);
        auto free = earliestFree(did, fromDay);

        if (sev == Severity::EMERGENCY && (free.first == -1 || free.first > fromDay)) {
            // nobody free that day -> bump the lowest-priority appointment on fromDay
            Appointment* victim = nullptr;
            for (int s = SLOTS - 1; s >= 0; --s) {         // prefer late slots
                Appointment* a = appointmentAt(did, fromDay, s);
                if (a && a->getSeverity() < sev &&
                    (!victim || a->getSeverity() < victim->getSeverity()))
                    victim = a;
            }
            if (victim) {
                int slot = victim->getSlot();
                displace(*victim);
                int id = appointments.newId();
                appointments.add(Appointment(id, pid, did, fromDay, slot, sev));
                return id;
            }
        }
        if (free.first == -1)
            throw SlotUnavailableException("Doctor has no free slots in the booking window.");
        int id = appointments.newId();
        appointments.add(Appointment(id, pid, did, free.first, free.second, sev));
        return id;
    }

    void cancel(int apptId) {
        Appointment* a = appointments.find(apptId);
        if (!a || !a->isActive()) throw NotFoundException("Active appointment " + to_string(apptId));
        a->cancel();
    }

    // ---- UNIQUE FEATURE: wait-time estimator -----------------------------
    int estimateWaitMinutes(int did, int day, Severity sev) {
        if (!doctors.exists(did)) throw NotFoundException("Doctor " + to_string(did));
        validateSlot(day, 0);
        int ahead = 0;
        for (auto& kv : appointments.all()) {
            const Appointment& a = kv.second;
            if (a.isActive() && a.getDoctorId() == did && a.getDay() == day &&
                a.getSeverity() >= sev)
                ++ahead;
        }
        return ahead * SLOT_MINUTES;
    }

    // ---- UNIQUE FEATURE: triage queue (priority_queue) --------------------
    void showTriageQueue(int day) {
        priority_queue<Appointment> pq;
        for (auto& kv : appointments.all())
            if (kv.second.isActive() && kv.second.getDay() == day) pq.push(kv.second);
        if (pq.empty()) { cout << "No appointments on day " << day << ".\n"; return; }
        cout << "\n--- TRIAGE QUEUE FOR DAY " << day << " (order of treatment) ---\n";
        int n = 1;
        while (!pq.empty()) {
            const Appointment a = pq.top();
            pq.pop();
            Patient* p = patients.find(a.getPatientId());
            Doctor* d = doctors.find(a.getDoctorId());
            cout << right << setw(3) << n++ << ". " << left << setw(10) << severityName(a.getSeverity())
                 << setw(18) << (p ? p->getName() : "?") << "Dr. "
                 << setw(16) << (d ? d->getName() : "?") << slotTime(a.getSlot()) << "\n";
        }
    }

    // ---- views --------------------------------------------------------------
    void showPatients() {
        cout << "\n" << left << setw(5) << "ID" << setw(20) << "Name" << setw(6) << "Age"
             << setw(14) << "Phone" << "Ailment\n" << string(62, '-') << "\n";
        for (auto& kv : patients.all()) kv.second.display();   // polymorphic call
        if (patients.size() == 0) cout << "(none)\n";
    }
    void showDoctors() {
        cout << "\n" << left << setw(5) << "ID" << setw(20) << "Name" << setw(18)
             << "Specialization" << setw(14) << "Phone" << "Fee\n" << string(66, '-') << "\n";
        for (auto& kv : doctors.all()) kv.second.display();
        if (doctors.size() == 0) cout << "(none)\n";
    }
    void showDoctorSchedule(int did, int day) {
        Doctor* d = doctors.find(did);
        if (!d) throw NotFoundException("Doctor " + to_string(did));
        validateSlot(day, 0);
        cout << "\nSchedule: Dr. " << d->getName() << " (" << d->getSpecialization()
             << ") - Day " << day << "\n" << string(50, '-') << "\n";
        for (int s = 0; s < SLOTS; ++s) {
            cout << slotTime(s) << "  ";
            Appointment* a = appointmentAt(did, day, s);
            if (!a) cout << "[ free ]\n";
            else {
                Patient* p = patients.find(a->getPatientId());
                cout << (p ? p->getName() : "?") << "  [" << severityName(a->getSeverity()) << "]\n";
            }
        }
    }
    void showPatientAppointments(int pid) {
        if (!patients.exists(pid)) throw NotFoundException("Patient " + to_string(pid));
        bool any = false;
        for (auto& kv : appointments.all())
            if (kv.second.isActive() && kv.second.getPatientId() == pid) {
                cout << kv.second << "\n";
                any = true;
            }
        if (!any) cout << "No active appointments for this patient.\n";
    }
    void searchPatient(const string& key) {
        string k = key;
        transform(k.begin(), k.end(), k.begin(), ::tolower);
        bool any = false;
        for (auto& kv : patients.all()) {
            string n = kv.second.getName();
            transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.find(k) != string::npos) { kv.second.display(); any = true; }
        }
        if (!any) cout << "No match.\n";
    }

    // ---- report -------------------------------------------------------------
    void showReport() {
        int total = 0, r = 0, u = 0, e = 0, moved = 0;
        double revenue = 0;
        map<int, int> perDoctor;
        for (auto& kv : appointments.all()) {
            const Appointment& a = kv.second;
            if (!a.isActive()) continue;
            ++total;
            perDoctor[a.getDoctorId()]++;
            if (a.getRescheduledCount() > 0) ++moved;
            if (a.getSeverity() == Severity::ROUTINE) ++r;
            else if (a.getSeverity() == Severity::URGENT) ++u;
            else ++e;
            Doctor* d = doctors.find(a.getDoctorId());
            if (d) revenue += d->getFee();
        }
        cout << "\n========== HOSPITAL REPORT ==========\n"
             << "Patients registered : " << patients.size() << "\n"
             << "Doctors on staff    : " << doctors.size() << "\n"
             << "Active appointments : " << total << "\n"
             << "  Routine / Urgent / Emergency : " << r << " / " << u << " / " << e << "\n"
             << "Rescheduled by triage: " << moved << "\n"
             << "Expected revenue    : Rs " << fixed << setprecision(0) << revenue << "\n"
             << "Appointments created (session): " << Appointment::getTotalCreated() << "\n";
        if (!perDoctor.empty()) {
            auto best = max_element(perDoctor.begin(), perDoctor.end(),
                [](const pair<const int, int>& a, const pair<const int, int>& b) { return a.second < b.second; });
            Doctor* d = doctors.find(best->first);
            cout << "Busiest doctor      : Dr. " << (d ? d->getName() : "?") << " (" << best->second << " appts)\n";
        }
        cout << "=====================================\n";
    }

    // ---- persistence --------------------------------------------------------
    void save() {
        ofstream p("patients.txt"), d("doctors.txt"), a("appointments.txt");
        if (!p || !d || !a) throw HospitalException("Could not open data files for writing.");
        for (auto& kv : patients.all()) p << kv.second.serialize() << "\n";
        for (auto& kv : doctors.all()) d << kv.second.serialize() << "\n";
        for (auto& kv : appointments.all()) a << kv.second.serialize() << "\n";
    }
    void load() {
        string line;
        ifstream p("patients.txt"), d("doctors.txt"), a("appointments.txt");
        while (getline(p, line)) if (!line.empty()) patients.add(Patient::deserialize(line));
        while (getline(d, line)) if (!line.empty()) doctors.add(Doctor::deserialize(line));
        while (getline(a, line)) if (!line.empty()) appointments.add(Appointment::deserialize(line));
    }

    void loadDemoData() {
        int d1 = addDoctor("Anil Sharma", 45, "9810000001", "Cardiology", 800);
        int d2 = addDoctor("Meera Kapoor", 38, "9810000002", "General Medicine", 400);
        addDoctor("Rahul Verma", 41, "9810000003", "Orthopedics", 600);
        int p1 = addPatient("Amit Singh", 30, "9876500001", "Fever");
        int p2 = addPatient("Neha Gupta", 25, "9876500002", "Back pain");
        int p3 = addPatient("Ravi Kumar", 58, "9876500003", "Chest pain");
        int p4 = addPatient("Simran Kaur", 34, "9876500004", "Checkup");
        for (int s = 0; s < SLOTS; ++s) {          // fill Dr. Meera's day 1 fully
            int pid = (s % 2 == 0) ? p1 : p2;
            book(pid, d2, 1, s, s < 3 ? Severity::URGENT : Severity::ROUTINE);
        }
        book(p4, d1, 1, 0, Severity::ROUTINE);
        (void)p3;
    }
};

// ============================================================ input helpers
string readLine(const string& prompt) {
    cout << prompt;
    string s;
    getline(cin, s);
    return s;
}
int readInt(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        try {
            size_t pos;
            int v = stoi(s, &pos);
            if (pos != s.size()) throw invalid_argument("x");
            return v;
        } catch (...) {
            if (cin.eof()) throw HospitalException("Input ended.");
            cout << "  Please enter a valid number.\n";
        }
    }
}
Severity readSeverity() {
    int v = readInt("Severity (1=Routine, 2=Urgent, 3=EMERGENCY): ");
    if (v < 1 || v > 3) throw InvalidInputException("Severity must be 1, 2 or 3.");
    return static_cast<Severity>(v);
}

void printMenu() {
    cout << "\n=========== HOSPITAL APPOINTMENT SYSTEM ===========\n"
         << " 1. Register patient          9. Doctor schedule (day view)\n"
         << " 2. Register doctor          10. Patient's appointments\n"
         << " 3. List patients            11. Search patient by name\n"
         << " 4. List doctors             12. Hospital report\n"
         << " 5. Book (choose slot)       13. Triage queue for a day\n"
         << " 6. SMART AUTO-BOOK          14. Wait-time estimator\n"
         << " 7. Cancel appointment       15. Load demo data\n"
         << " 0. Save & exit\n"
         << "===================================================\n";
}

int main() {
    Hospital h;
    try { h.load(); } catch (const exception& e) { cout << "Load warning: " << e.what() << "\n"; }

    while (true) {
        printMenu();
        int choice;
        try { choice = readInt("Choice: "); }
        catch (const HospitalException&) { choice = 0; }

        try {
            switch (choice) {
                case 0:
                    h.save();
                    cout << "Data saved. Goodbye!\n";
                    return 0;
                case 1: {
                    string n = readLine("Name: ");
                    int age = readInt("Age: ");
                    string ph = readLine("Phone: ");
                    string ail = readLine("Ailment: ");
                    int nid = h.addPatient(n, age, ph, ail);
                    cout << "Patient registered with ID " << nid << "\n";
                    break;
                }
                case 2: {
                    string n = readLine("Name: ");
                    int age = readInt("Age: ");
                    string ph = readLine("Phone: ");
                    string sp = readLine("Specialization: ");
                    int fee = readInt("Consultation fee: ");
                    int nid = h.addDoctor(n, age, ph, sp, fee);
                    cout << "Doctor registered with ID " << nid << "\n";
                    break;
                }
                case 3: h.showPatients(); break;
                case 4: h.showDoctors(); break;
                case 5: {
                    int pid = readInt("Patient ID: "), did = readInt("Doctor ID: ");
                    int day = readInt("Day (1-30): "), slot = readInt("Slot (0-7, 0 = 09:00): ");
                    Severity s = readSeverity();
                    int aid = h.book(pid, did, day, slot, s);
                    cout << "Booked. Appointment ID " << aid << "\n";
                    break;
                }
                case 6: {
                    int pid = readInt("Patient ID: "), did = readInt("Doctor ID: ");
                    int day = readInt("Earliest day you can come (1-30): ");
                    Severity s = readSeverity();
                    int aid = h.autoBook(pid, did, s, day);
                    cout << "Auto-booked. Appointment ID " << aid << "\n";
                    break;
                }
                case 7: h.cancel(readInt("Appointment ID: ")); cout << "Cancelled.\n"; break;
                case 9: {
                    int did = readInt("Doctor ID: "), day = readInt("Day (1-30): ");
                    h.showDoctorSchedule(did, day);
                    break;
                }
                case 10: h.showPatientAppointments(readInt("Patient ID: ")); break;
                case 11: h.searchPatient(readLine("Name contains: ")); break;
                case 12: h.showReport(); break;
                case 13: h.showTriageQueue(readInt("Day (1-30): ")); break;
                case 14: {
                    int did = readInt("Doctor ID: "), day = readInt("Day (1-30): ");
                    Severity s = readSeverity();
                    int w = h.estimateWaitMinutes(did, day, s);
                    cout << "Estimated wait: " << w << " minutes\n";
                    break;
                }
                case 15: h.loadDemoData(); cout << "Demo data loaded.\n"; break;
                default: cout << "Invalid choice.\n";
            }
        } catch (const HospitalException& e) {
            cout << "  [!] " << e.what() << "\n";
        } catch (const exception& e) {
            cout << "  [!] Unexpected error: " << e.what() << "\n";
        }
    }
}
