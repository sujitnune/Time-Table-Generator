# 📅 Automatic Timetable Generator

A C-based application that automates weekly academic timetable creation for multiple sections in educational institutions. Built using **graph structures**, **Greedy algorithms**, and a hint of **Backtracking**, it ensures efficient, conflict-free scheduling.

---

## 🔧 Features

- Supports **theory (1-hour)** & **lab (2-hour)** classes  
- Avoids **faculty conflicts** and **double bookings**  
- Respects **faculty availability**, **subject frequency**, and **lunch breaks**  
- Generates **section-wise timetables** and **faculty workload reports**

---

## ⚙️ How It Works

### 📥 Input
- Reads `subjects.txt` with subject name, duration, faculty, and weekly frequency.

### 🧠 Scheduling
- Uses a **faculty availability matrix**
- Applies a **Greedy algorithm** to place subjects by priority
- Prevents **clashes in time slots and sections**

### 📤 Output
- Saves timetables per section and a **faculty workload summary**
