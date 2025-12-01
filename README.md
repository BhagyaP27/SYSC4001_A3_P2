# SYSC 4001 Assignment 3 - Part 2: Concurrent TA Marking System

## Authors
- Student 1: Bhagya Patel (101324150)
- Student 2: [Name, ID]

## Overview
This system simulates multiple Teaching Assistants (TAs) concurrently marking exams using shared memory and semaphores.

## Compilation
```bash
make all
```

This creates two executables:
- `ta_marking_a` - Part A: Without synchronization (race conditions possible)
- `ta_marking_b` - Part B: With semaphores (synchronized)

## Setup Test Files
```bash
chmod +x create_test_files.sh
make setup
```

This creates:
- 20 exam files (exam_0001.txt to exam_0020.txt)
- 1 termination file (exam_9999.txt)
- rubric.txt file

## Running

### Part A (Without Semaphores)
```bash
./ta_marking_a <number_of_TAs>

# Example with 3 TAs:
./ta_marking_a 3
```

Expected: Race conditions may occur (TAs marking same question, concurrent rubric writes)

### Part B (With Semaphores)
```bash
./ta_marking_b <number_of_TAs>

# Example with 3 TAs:
./ta_marking_b 3
```

Expected: No race conditions, proper synchronization

## Cleaning Up
```bash
make clean
```

This removes:
- Compiled executables
- Shared memory segments
- Semaphores

## Design Decisions

### Shared Memory Structure
```c
struct SharedMemory {
    char current_exam[100];      // Current exam being marked
    int exam_number;              // Current exam number
    bool questions_marked[5];     // Track which questions are marked
    bool all_questions_done;      // All questions marked flag
    char rubric[5][50];          // Rubric data
    int rubric_version;           // Track rubric changes
};
```

### Semaphores (Part B)
1. **rubric_mutex** - Protects rubric reads/writes (readers-writer pattern)
2. **rubric_write_mutex** - Ensures only one TA writes rubric
3. **exam_mutex** - Protects exam file access
4. **questions_mutex** - Protects question marking array

### Critical Sections
1. **Reading Rubric** - Multiple TAs can read concurrently
2. **Writing Rubric** - Only one TA at a time
3. **Marking Questions** - Prevent duplicate marking
4. **Loading Next Exam** - One TA loads, others wait

## Test Cases

### Test 1: Two TAs
```bash
./ta_marking_b 2
```
Expected: Both TAs share work, no conflicts

### Test 2: Five TAs
```bash
./ta_marking_b 5
```
Expected: More parallelism, still synchronized

### Test 3: Race Conditions (Part A)
```bash
./ta_marking_a 3
```
Expected: May see duplicate markings, concurrent rubric changes

## Known Issues & Solutions

### Issue: "Permission denied" on semaphores
**Solution:** 
```bash
make clean
```

### Issue: Shared memory not cleaned
**Solution:**
```bash
ipcs -m | grep $USER | awk '{print $2}' | xargs -I {} ipcrm -m {}
```

## Performance Analysis

See `reportPartC.pdf` for:
- Execution order analysis
- Deadlock/livelock discussion
- Critical section requirements
- Performance comparison (Part A vs Part B)