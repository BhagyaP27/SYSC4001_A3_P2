# SYSC 4001 Assignment 3 - Part 2: Concurrent TA Marking System

**Team Members:**
- Student 1: Bhagya Patel - 101324150
- Student 2:Oluwatobi Olowookere - 101245900

---

## Overview

This implements a concurrent TA marking system where multiple TAs work simultaneously to mark exams. The system demonstrates process synchronization using shared memory and semaphores.

**Key Features:**
- Multiple concurrent TA processes (not threads - using `fork()`)
- Shared memory for rubric and exam files
- Readers-writers pattern for rubric access
- Each TA marks ONE question per exam cycle
- Random delays simulate realistic marking times

---

## Files Included

```
SYSC4001_A3P2/
├── ta_marking_2a_<student1>_<student2>.cpp  # Part 2a (without semaphores)
├── ta_marking_2b_<student1>_<student2>.cpp  # Part 2b (with semaphores)
├── setup_test_files.sh                       # Creates test files
├── Makefile                                   # Build script
├── README.md                                  # This file
└── PartC.pdf                                  # Analysis report
```

---

##  Quick Start

### 1. Create Test Files

```bash
chmod +x setup_test_files.sh
./setup_test_files.sh
```

This creates:
- `rubric.txt` - 5 rubric entries (1,A through 5,E)
- `exam_0001.txt` to `exam_0025.txt` - 25 exam files
- `exam_9999.txt` - Special termination file

### 2. Compile

**Option A: Using Makefile**
```bash
make all
```

**Option B: Manual Compilation**
```bash
# Replace <student1> and <student2> with your actual student numbers!
g++ -o ta_marking_2a ta_marking_2a_<student1>_<student2>.cpp -lrt -lpthread -std=c++11
g++ -o ta_marking_2b ta_marking_2b_<student1>_<student2>.cpp -lrt -lpthread -std=c++11
```

### 3. Run the Programs

**Part 2a (Without Semaphores - Expect Race Conditions)**
```bash
./ta_marking_2a 3
```

**Part 2b (With Semaphores - Properly Synchronized)**
```bash
./ta_marking_2b 3
```

---

## 📖 How It Works

### The Scenario

Multiple Teaching Assistants (TAs) are marking exams concurrently. Each exam has 5 questions.

**TA Workflow:**
1. **Review Rubric** - Check all 5 rubric lines (0.5-1.0s per line)
   - 30% chance of detecting an error
   - If error found, correct it (increment ASCII character)
   
2. **Pick an Exam** - Find an exam with unmarked questions

3. **Mark ONE Question** - Select and mark one question (1.0-2.0s)
   - Display: "Marking question X for student YYYY"
   
4. **Repeat** - Go back to step 1 until student 9999 is found

### Concurrency Rules

✅ **Rubric Reading**: Multiple TAs can read simultaneously  
✅ **Rubric Writing**: Only ONE TA can write at a time  
✅ **Question Marking**: Each question marked by exactly one TA  
✅ **Exam Loading**: Only one TA loads new exams  

### Key Differences: Part 2a vs 2b

| Aspect | Part 2a (No Semaphores) | Part 2b (With Semaphores) |
|--------|------------------------|---------------------------|
| Rubric Access | ❌ Race conditions | ✅ Readers-writers pattern |
| Question Selection | ❌ Multiple TAs might mark same question | ✅ Mutex protection |
| Exam Loading | ❌ Multiple TAs might load same exam | ✅ Mutex protection |
| Correctness | ❌ Incorrect behavior | ✅ Correct synchronization |

---

## 🔧 Implementation Details

### Shared Memory Structure

```cpp
struct SharedData {
    char rubric[MAX_RUBRIC_SIZE];          // Rubric in memory
    ExamData exams[MAX_EXAMS];             // Multiple exams
    int total_exams_loaded;
    int next_exam_to_load;
    bool all_done;                         // Termination flag
    
    // Semaphores (Part 2b only)
    sem_t rubric_mutex;                    // Writers lock
    sem_t reader_count_mutex;              // Reader counter lock
    int reader_count;                      // Active readers
    sem_t exam_load_mutex;                 // Exam loading lock
};

struct ExamData {
    char exam_content[MAX_EXAM_SIZE];
    int student_number;
    bool questions_marked[5];              // Track each question
    int questions_completed;
    sem_t exam_mutex;                      // Per-exam lock (Part 2b)
};
```

### Synchronization Strategy (Part 2b)

#### 1. Readers-Writers for Rubric

**Reading** (multiple TAs can read concurrently):
```cpp
sem_wait(&reader_count_mutex);
reader_count++;
if (reader_count == 1)
    sem_wait(&rubric_mutex);  // First reader blocks writers
sem_post(&reader_count_mutex);

// Read rubric...

sem_wait(&reader_count_mutex);
reader_count--;
if (reader_count == 0)
    sem_post(&rubric_mutex);  // Last reader unblocks writers
sem_post(&reader_count_mutex);
```

**Writing** (only one TA can write):
```cpp
sem_wait(&rubric_mutex);      // Exclusive access
// Modify rubric...
sem_post(&rubric_mutex);
```

#### 2. Per-Exam Mutex

Each exam has its own mutex to prevent race conditions when marking:
```cpp
sem_wait(&exam->exam_mutex);
// Select unmarked question
// Mark it as "being worked on"
sem_post(&exam->exam_mutex);

// Do actual marking (NO LOCK HELD - allows parallelism)

sem_wait(&exam->exam_mutex);
// Update completion count
sem_post(&exam->exam_mutex);
```

#### 3. Exam Loading Mutex

Ensures only one TA loads the next exam:
```cpp
sem_wait(&exam_load_mutex);
// Load next exam file into shared memory
sem_post(&exam_load_mutex);
```

---

##  Testing

### Test Case 1: Race Conditions (Part 2a)

```bash
./ta_marking_2a 3
```

**Expected Observations:**
- ❌ Multiple TAs might mark the same question
- ❌ Lost rubric updates
- ❌ Multiple TAs might load the same exam
- ❌ Inconsistent completion counts

### Test Case 2: Proper Synchronization (Part 2b)

```bash
./ta_marking_2b 3
```

**Expected Observations:**
- ✅ Each question marked exactly once
- ✅ Rubric updates properly serialized
- ✅ "active readers: X" shown during rubric access
- ✅ "Acquired write lock" messages when correcting rubric
- ✅ Single exam loading per file

### Test Case 3: Scalability

```bash
./ta_marking_2b 2   # Lower concurrency
./ta_marking_2b 5   # High concurrency - all 5 questions can be marked simultaneously
./ta_marking_2b 10  # Very high concurrency
```

### Comparison Test

```bash
make compare
# Runs both 2a and 2b back-to-back for comparison
```

---

##  What to Look For

### Part 2a Output (Race Conditions)

```
[TA 1] Marking question 3 for student 0001
[TA 2] Marking question 3 for student 0001  ← RACE CONDITION!
[TA 1] Changed 'A' to 'B' in question 1
[TA 2] Changed 'A' to 'B' in question 1     ← LOST UPDATE!
```

### Part 2b Output (Synchronized)

```
[TA 1] Reading rubric (active readers: 1)
[TA 2] Reading rubric (active readers: 2)   ← Multiple readers OK
[TA 1] Requesting WRITE access to rubric
[TA 1] Acquired write lock                  ← Exclusive write
[TA 1] Changed 'A' to 'B' in question 1
[TA 1] Released write lock
[TA 2] Marking question 1 for student 0001
[TA 3] Marking question 2 for student 0001  ← Different questions
```

---

##  Troubleshooting

### Problem: "Permission denied"
```bash
chmod +x ta_marking_2a ta_marking_2b setup_test_files.sh
```

### Problem: "Cannot open rubric.txt"
```bash
./setup_test_files.sh  # Recreate files
ls *.txt               # Verify they exist
```

### Problem: "shm_open failed"
```bash
# Clean up old shared memory
rm /dev/shm/ta_marking_shm
# Or use:
make clean
```

### Problem: Program hangs
```bash
# Kill with Ctrl+C
# Clean up:
killall ta_marking_2a ta_marking_2b
rm /dev/shm/ta_marking_shm
```

### Problem: Compilation errors
```bash
# Ubuntu/Debian:
sudo apt-get install build-essential

# Make sure you're using C++11:
g++ -std=c++11 -o ta_marking_2b ta_marking_2b_*.cpp -lrt -lpthread
```

---

##  Part 2c - What to Write

Your `reportPartC.pdf` should discuss:

### 1. Execution Observations
- Did you observe deadlock? (No, because no nested locks)
- Did you observe livelock? (No, because blocking semaphores)
- Describe execution order and non-determinism

### 2. Deadlock Analysis
- **Four conditions needed**: Mutual exclusion, Hold-and-wait, No preemption, Circular wait
- **Why no deadlock**: Single lock acquisitions prevent circular wait
- **Only exception**: Process crash while holding lock

### 3. Livelock Analysis
- **Definition**: Processes active but not making progress
- **Why no livelock**: Using `sem_wait()` (blocking) not `sem_trywait()` (busy-wait)

### 4. Execution Order Discussion
- Non-deterministic: Which TA marks which question varies
- Deterministic: Each question marked exactly once
- Provide example traces from your runs

---

## ✅ Critical Section Requirements

### 1. Mutual Exclusion ✅
- Only one TA writes to rubric at a time
- Only one TA selects from each exam at a time
- Only one TA loads new exams at a time

### 2. Progress ✅
- No process holds locks indefinitely
- Work (marking) done outside critical sections
- FIFO queuing ensures eventual access

### 3. Bounded Waiting ✅
- Maximum wait = (n-1) other TAs
- POSIX semaphores use fair queuing
- No starvation possible

---

## 📦 Submission Checklist

Before pushing to GitHub:

- [ ] Both programs compile without warnings
- [ ] Replaced `<student1>` and `<student2>` with actual student numbers
- [ ] Part 2a shows expected race conditions
- [ ] Part 2b runs correctly without race conditions
- [ ] At least 20 exam files + exam_9999.txt exist
- [ ] README.md complete
- [ ] reportPartC.pdf written and explains deadlock/livelock
- [ ] All files in GitHub repository: **SYSC4001_A3P2**
- [ ] Code is well-commented
- [ ] Tested with 2, 3, and 5 TAs

---

## 🎓 Grading Rubric (Part 2: 1.0 mark total)

**Part 2a (0.5 marks):**
- ✅ Multiple processes created correctly
- ✅ Shared memory used
- ✅ Each TA prints what it's doing
- ✅ Race conditions present (expected and OK)
- ✅ Processes don't just create files and exit

**Part 2b (0.3 marks):**
- ✅ Semaphores implemented correctly
- ✅ Readers-writers for rubric
- ✅ Mutual exclusion for question selection
- ✅ Mutual exclusion for exam loading
- ✅ No race conditions

**Part 2c (0.2 marks):**
- ✅ Deadlock analysis complete
- ✅ Livelock analysis complete
- ✅ Execution order discussed
- ✅ Critical section requirements verified

---

##  Useful Commands

```bash
# Build everything
make all

# Create test files
make setup

# Run Part 2a
make run2a
# or
./ta_marking_2a 3

# Run Part 2b  
make run2b
# or
./ta_marking_2b 3

# Compare both versions
make compare

# Test with different TA counts
make test

# Clean up
make clean

# Remove everything including test files
make distclean
```

---

##  References

- Assignment 3 specification (Part 2, pages 8-10)
- Silberschatz, Galvin, Gagne: *Operating System Concepts*, Chapter 6 (Synchronization)
- POSIX Semaphores: `man sem_init`, `man sem_wait`, `man sem_post`
- Shared Memory: `man shm_open`, `man mmap`

---

##  Tips for Success

1. **Start with Part 2a** - Get the basic process structure working first
2. **Add lots of print statements** - See what's happening
3. **Test with 2 TAs first** - Easier to debug
4. **Watch for race conditions in 2a** - They prove you understand the problem
5. **Verify synchronization in 2b** - Each question should be marked exactly once
6. **Use `strace -f ./ta_marking_2b 3`** - See all system calls
7. **Read the assignment PDF carefully** - Requirements are specific

---

##  Time Estimates

- Understanding requirements: 30 min
- Part 2a implementation: 2-3 hours
- Part 2b implementation: 2-3 hours
- Testing and debugging: 1-2 hours
- Part 2c report: 1 hour
- **Total: 6-9 hours**

---
