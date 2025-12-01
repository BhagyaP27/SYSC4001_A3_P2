/**
 * @file ta_marking_part_a.cpp
 * @brief Part A: TA Marking System WITHOUT Semaphores
 * @author Student 1: Bhagya Patel (101324150)
 * @author Student 2: [Name, ID]
 * 
 * This version allows race conditions to demonstrate the need for synchronization
 */

#include <iostream>
#include <fstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <sstream>

using namespace std;

// Shared memory structure
struct SharedMemory {
    char current_exam[100];           // Current exam student number
    int exam_number;                  // Current exam index
    bool questions_marked[5];         // Which questions are marked
    bool all_questions_done;          // All questions for current exam done
    char rubric[5][50];              // Rubric lines
    int rubric_version;               // Track rubric updates
    bool terminate;                   // Termination flag
};

// Random delay between min and max seconds
void random_delay(double min_sec, double max_sec) {
    double range = max_sec - min_sec;
    double random = ((double)rand() / RAND_MAX) * range + min_sec;
    usleep((int)(random * 1000000));
}

// Load rubric from file into shared memory
void load_rubric(SharedMemory* shm) {
    ifstream rubric_file("rubric.txt");
    if (!rubric_file.is_open()) {
        cerr << "Error: Cannot open rubric.txt" << endl;
        return;
    }
    
    string line;
    int i = 0;
    while (getline(rubric_file, line) && i < 5) {
        strcpy(shm->rubric[i], line.c_str());
        i++;
    }
    rubric_file.close();
}

// Load exam into shared memory
bool load_exam(SharedMemory* shm, int exam_num) {
    char filename[100];
    sprintf(filename, "exams/exam_%04d.txt", exam_num);
    
    ifstream exam_file(filename);
    if (!exam_file.is_open()) {
        return false;
    }
    
    string line;
    getline(exam_file, line);
    
    // Extract student number
    size_t pos = line.find("Student:");
    if (pos != string::npos) {
        string student_num = line.substr(pos + 9);
        // Trim whitespace
        student_num.erase(0, student_num.find_first_not_of(" \t"));
        student_num.erase(student_num.find_last_not_of(" \t\n\r") + 1);
        strcpy(shm->current_exam, student_num.c_str());
    }
    
    exam_file.close();
    
    // Reset question flags
    for (int i = 0; i < 5; i++) {
        shm->questions_marked[i] = false;
    }
    shm->all_questions_done = false;
    
    return true;
}

// TA process function
void ta_process(int ta_id, SharedMemory* shm) {
    srand(time(NULL) + ta_id);
    
    cout << "[TA " << ta_id << "] Started marking" << endl;
    
    while (!shm->terminate) {
        // Check if current exam is done
        if (shm->all_questions_done) {
            usleep(100000); // Wait for next exam
            continue;
        }
        
        // Review and potentially correct rubric
        cout << "[TA " << ta_id << "] Reviewing rubric for exam " 
             << shm->current_exam << endl;
        
        for (int q = 0; q < 5; q++) {
            random_delay(0.5, 1.0);
            
            // Randomly decide to correct rubric
            if (rand() % 10 < 3) { // 30% chance
                // WARNING: Race condition possible here!
                cout << "[TA " << ta_id << "] Correcting rubric line " << (q + 1) << endl;
                
                // Parse rubric line
                char line[50];
                strcpy(line, shm->rubric[q]);
                
                // Find the character after comma
                char* comma = strchr(line, ',');
                if (comma && *(comma + 2)) {
                    *(comma + 2) = *(comma + 2) + 1; // Increment ASCII
                    strcpy(shm->rubric[q], line);
                    shm->rubric_version++;
                    
                    // Save to file (WARNING: Race condition!)
                    ofstream rubric_file("rubric.txt");
                    for (int i = 0; i < 5; i++) {
                        rubric_file << shm->rubric[i] << endl;
                    }
                    rubric_file.close();
                    
                    cout << "[TA " << ta_id << "] Updated rubric to: " << line << endl;
                }
            }
        }
        
        // Try to mark a question
        bool marked_something = false;
        for (int q = 0; q < 5; q++) {
            // WARNING: Race condition - multiple TAs might see same unmarked question
            if (!shm->questions_marked[q]) {
                cout << "[TA " << ta_id << "] Marking question " << (q + 1) 
                     << " for student " << shm->current_exam << endl;
                
                // Mark the question (WARNING: Race condition!)
                shm->questions_marked[q] = true;
                
                // Simulate marking time
                random_delay(1.0, 2.0);
                
                cout << "[TA " << ta_id << "] Finished marking question " << (q + 1) 
                     << " for student " << shm->current_exam << endl;
                
                marked_something = true;
                break;
            }
        }
        
        // Check if all questions are done
        bool all_done = true;
        for (int q = 0; q < 5; q++) {
            if (!shm->questions_marked[q]) {
                all_done = false;
                break;
            }
        }
        
        if (all_done && !shm->all_questions_done) {
            shm->all_questions_done = true;
            
            // Load next exam (WARNING: Race condition - multiple TAs might try!)
            int next_exam = shm->exam_number + 1;
            cout << "[TA " << ta_id << "] Loading next exam " << next_exam << endl;
            
            if (load_exam(shm, next_exam)) {
                shm->exam_number = next_exam;
                
                // Check for termination
                if (strcmp(shm->current_exam, "9999") == 0) {
                    cout << "[TA " << ta_id << "] Found termination exam 9999" << endl;
                    shm->terminate = true;
                    break;
                }
            }
        }
        
        if (!marked_something) {
            usleep(100000); // Brief wait if nothing to do
        }
    }
    
    cout << "[TA " << ta_id << "] Finished marking" << endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <number_of_TAs>" << endl;
        return 1;
    }
    
    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        cerr << "Error: Need at least 2 TAs" << endl;
        return 1;
    }
    
    cout << "=== TA Marking System - Part A (WITHOUT Semaphores) ===" << endl;
    cout << "Number of TAs: " << num_tas << endl;
    cout << "WARNING: Race conditions may occur!" << endl << endl;
    
    // Create shared memory
    int shm_fd = shm_open("/ta_marking_shm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }
    
    ftruncate(shm_fd, sizeof(SharedMemory));
    
    SharedMemory* shm = (SharedMemory*)mmap(NULL, sizeof(SharedMemory),
                                             PROT_READ | PROT_WRITE,
                                             MAP_SHARED, shm_fd, 0);
    
    if (shm == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    
    // Initialize shared memory
    memset(shm, 0, sizeof(SharedMemory));
    load_rubric(shm);
    load_exam(shm, 1);
    shm->exam_number = 1;
    
    cout << "Starting with exam: " << shm->current_exam << endl << endl;
    
    // Fork TA processes
    vector<pid_t> ta_pids;
    for (int i = 0; i < num_tas; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            ta_process(i + 1, shm);
            exit(0);
        } else if (pid > 0) {
            ta_pids.push_back(pid);
        } else {
            perror("fork");
            return 1;
        }
    }
    
    // Wait for all TAs to finish
    for (pid_t pid : ta_pids) {
        waitpid(pid, NULL, 0);
    }
    
    cout << endl << "=== All TAs finished ===" << endl;
    
    // Cleanup
    munmap(shm, sizeof(SharedMemory));
    shm_unlink("/ta_marking_shm");
    
    return 0;
}