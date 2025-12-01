/**
 * @file ta_marking_part_a.cpp
 * @brief Part A: TA Marking System WITHOUT Semaphores
 * @author Student 1: Bhagya Patel (101324150)
 * @author Student 2: [Name, ID]
 * 
 * This version allows Concurrent TA marking WITH semaphores (properly synchronized)
 */
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <random>
#include <dirent.h>
#include <algorithm>

#define MAX_RUBRIC_SIZE 2048
#define MAX_EXAM_SIZE 4096
#define MAX_EXAMS 100
#define NUM_QUESTIONS 5

// Structure for exam in shared memory
struct ExamData {
    char exam_content[MAX_EXAM_SIZE];
    int student_number;
    bool questions_marked[NUM_QUESTIONS];
    bool in_use;
    int questions_completed;
    sem_t exam_mutex;  // Mutex for this specific exam
};

// Shared memory structure
struct SharedData {
    char rubric[MAX_RUBRIC_SIZE];
    ExamData exams[MAX_EXAMS];
    int total_exams_loaded;
    bool all_done;
    char exam_filenames[MAX_EXAMS][256];
    int num_exam_files;
    int next_exam_to_load;
    
    // Synchronization primitives
    sem_t rubric_mutex;        // For rubric writes (readers-writer)
    sem_t reader_count_mutex;  // Protects reader_count
    int reader_count;          // Number of active readers
    sem_t exam_load_mutex;     // For loading new exams
};

// Get random delay
double get_random_delay(double min_sec, double max_sec) {
    static thread_local std::mt19937 gen(std::random_device{}() + getpid());
    std::uniform_real_distribution<> dis(min_sec, max_sec);
    return dis(gen);
}

// Load rubric from file
void load_rubric(SharedData* shared) {
    std::ifstream file("rubric.txt");
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open rubric.txt\n";
        exit(1);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    strncpy(shared->rubric, content.c_str(), MAX_RUBRIC_SIZE - 1);
    shared->rubric[MAX_RUBRIC_SIZE - 1] = '\0';
    file.close();
}

// Save rubric to file
void save_rubric(SharedData* shared) {
    std::ofstream file("rubric.txt");
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write rubric.txt\n";
        return;
    }
    file << shared->rubric;
    file.close();
}

// Load an exam file into shared memory
bool load_exam_into_memory(SharedData* shared, const std::string& filename, int exam_slot) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Read entire file
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // Parse student number (first line)
    size_t first_newline = content.find('\n');
    if (first_newline == std::string::npos) return false;
    
    int student_num = std::stoi(content.substr(0, first_newline));
    
    // Store in shared memory
    strncpy(shared->exams[exam_slot].exam_content, content.c_str(), MAX_EXAM_SIZE - 1);
    shared->exams[exam_slot].exam_content[MAX_EXAM_SIZE - 1] = '\0';
    shared->exams[exam_slot].student_number = student_num;
    shared->exams[exam_slot].in_use = false;
    shared->exams[exam_slot].questions_completed = 0;
    
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        shared->exams[exam_slot].questions_marked[i] = false;
    }
    
    // Initialize exam mutex
    sem_init(&shared->exams[exam_slot].exam_mutex, 1, 1);
    
    return true;
}

// Review rubric and potentially correct it (WITH SYNCHRONIZATION)
void review_and_correct_rubric(SharedData* shared, int ta_id) {
    // READERS-WRITERS PATTERN: Reading phase
    std::cout << "[TA " << ta_id << "] Requesting read access to rubric\n";
    
    sem_wait(&shared->reader_count_mutex);
    shared->reader_count++;
    if (shared->reader_count == 1) {
        // First reader locks out writers
        sem_wait(&shared->rubric_mutex);
    }
    sem_post(&shared->reader_count_mutex);
    
    std::cout << "[TA " << ta_id << "] Reading rubric (active readers: " 
              << shared->reader_count << ")\n";
    
    // CRITICAL SECTION: Reading rubric
    std::istringstream iss(shared->rubric);
    std::string line;
    std::vector<std::string> lines;
    
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    
    bool needs_correction = false;
    int line_to_correct = -1;
    
    // Iterate through each question in rubric
    for (size_t i = 0; i < lines.size() && i < NUM_QUESTIONS; i++) {
        std::cout << "[TA " << ta_id << "] Reviewing rubric question " << (i+1) << "...\n";
        usleep(get_random_delay(0.5, 1.0) * 1000000);
        
        // Randomly decide if correction needed
        if (rand() % 100 < 30) {
            std::cout << "[TA " << ta_id << "] Detected error in rubric question " << (i+1) << "\n";
            needs_correction = true;
            line_to_correct = i;
            break;
        }
    }
    
    // Release read lock
    sem_wait(&shared->reader_count_mutex);
    shared->reader_count--;
    if (shared->reader_count == 0) {
        // Last reader unlocks writers
        sem_post(&shared->rubric_mutex);
    }
    sem_post(&shared->reader_count_mutex);
    
    std::cout << "[TA " << ta_id << "] Finished reading rubric\n";
    
    // WRITERS PHASE: If correction needed
    if (needs_correction && line_to_correct >= 0) {
        std::cout << "[TA " << ta_id << "] Requesting WRITE access to rubric\n";
        
        // Acquire exclusive write lock
        sem_wait(&shared->rubric_mutex);
        
        std::cout << "[TA " << ta_id << "] Acquired write lock, correcting rubric\n";
        
        // CRITICAL SECTION: Writing to rubric
        // Re-parse in case it changed
        std::istringstream iss2(shared->rubric);
        lines.clear();
        while (std::getline(iss2, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        
        if (line_to_correct < (int)lines.size()) {
            std::string& target_line = lines[line_to_correct];
            size_t comma_pos = target_line.find(',');
            
            if (comma_pos != std::string::npos && comma_pos + 2 < target_line.length()) {
                char old_char = target_line[comma_pos + 2];
                target_line[comma_pos + 2] = old_char + 1;
                std::cout << "[TA " << ta_id << "] Changed '" << old_char << "' to '" 
                          << target_line[comma_pos + 2] << "' in question " 
                          << (line_to_correct + 1) << "\n";
            }
            
            // Rebuild rubric
            std::ostringstream new_rubric;
            for (const auto& l : lines) {
                new_rubric << l << "\n";
            }
            
            // Update shared memory
            strncpy(shared->rubric, new_rubric.str().c_str(), MAX_RUBRIC_SIZE - 1);
            shared->rubric[MAX_RUBRIC_SIZE - 1] = '\0';
            
            // Save to file
            save_rubric(shared);
            std::cout << "[TA " << ta_id << "] Saved corrected rubric to file\n";
        }
        
        // Release write lock
        sem_post(&shared->rubric_mutex);
        std::cout << "[TA " << ta_id << "] Released write lock\n";
    }
}

// Find an exam with unmarked questions
int find_exam_to_mark(SharedData* shared) {
    for (int i = 0; i < shared->total_exams_loaded; i++) {
        if (shared->exams[i].student_number != 9999 && 
            shared->exams[i].questions_completed < NUM_QUESTIONS) {
            return i;
        }
    }
    return -1;
}

// Mark one question on an exam (WITH SYNCHRONIZATION)
bool mark_one_question(SharedData* shared, int ta_id, int exam_idx) {
    // Lock this specific exam
    sem_wait(&shared->exams[exam_idx].exam_mutex);
    
    // Find an unmarked question
    int question_to_mark = -1;
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        if (!shared->exams[exam_idx].questions_marked[i]) {
            question_to_mark = i;
            shared->exams[exam_idx].questions_marked[i] = true;
            break;
        }
    }
    
    if (question_to_mark == -1) {
        // All questions already marked
        sem_post(&shared->exams[exam_idx].exam_mutex);
        return false;
    }
    
    int student_num = shared->exams[exam_idx].student_number;
    
    // Release lock before actual marking (don't hold lock during work)
    sem_post(&shared->exams[exam_idx].exam_mutex);
    
    std::cout << "[TA " << ta_id << "] Marking question " << (question_to_mark + 1) 
              << " for student " << student_num << "\n";
    
    // Marking time: 1.0-2.0 seconds (NO LOCK HELD)
    usleep(get_random_delay(1.0, 2.0) * 1000000);
    
    std::cout << "[TA " << ta_id << "] Finished marking question " << (question_to_mark + 1)
              << " for student " << student_num << "\n";
    
    // Update completion count with lock
    sem_wait(&shared->exams[exam_idx].exam_mutex);
    shared->exams[exam_idx].questions_completed++;
    sem_post(&shared->exams[exam_idx].exam_mutex);
    
    return true;
}

// TA process main function
void ta_process(SharedData* shared, int ta_id) {
    std::cout << "[TA " << ta_id << "] Started working\n";
    
    while (!shared->all_done) {
        // Step 1: Review rubric
        review_and_correct_rubric(shared, ta_id);
        
        // Step 2: Find an exam to mark
        int exam_idx = find_exam_to_mark(shared);
        
        if (exam_idx == -1) {
            // No exams available, try to load next one
            sem_wait(&shared->exam_load_mutex);
            
            // Double-check after acquiring lock
            exam_idx = find_exam_to_mark(shared);
            if (exam_idx == -1 && shared->next_exam_to_load < shared->num_exam_files) {
                int next_idx = shared->next_exam_to_load;
                shared->next_exam_to_load++;
                
                std::string filename = shared->exam_filenames[next_idx];
                std::cout << "[TA " << ta_id << "] Loading " << filename << " into shared memory\n";
                
                int slot = shared->total_exams_loaded;
                if (slot < MAX_EXAMS && load_exam_into_memory(shared, filename, slot)) {
                    shared->total_exams_loaded++;
                    
                    // Check if this is the termination exam
                    if (shared->exams[slot].student_number == 9999) {
                        std::cout << "[TA " << ta_id << "] Found student 9999 - signaling completion\n";
                        shared->all_done = true;
                    }
                }
            }
            
            sem_post(&shared->exam_load_mutex);
            
            if (exam_idx == -1 && shared->next_exam_to_load >= shared->num_exam_files) {
                usleep(100000);  // Wait if no work available
            }
        } else {
            // Step 3: Mark one question on the exam
            mark_one_question(shared, ta_id, exam_idx);
        }
        
        usleep(50000);  // Small delay
    }
    
    std::cout << "[TA " << ta_id << "] Finished working\n";
}

// Get list of exam files
void get_exam_files(SharedData* shared) {
    std::vector<std::string> files;
    
    DIR* dir = opendir(".");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename.find("exam_") == 0 && filename.find(".txt") != std::string::npos) {
                files.push_back(filename);
            }
        }
        closedir(dir);
    }
    
    std::sort(files.begin(), files.end());
    
    shared->num_exam_files = std::min((int)files.size(), MAX_EXAMS);
    for (int i = 0; i < shared->num_exam_files; i++) {
        strncpy(shared->exam_filenames[i], files[i].c_str(), 255);
        shared->exam_filenames[i][255] = '\0';
    }
    
    std::cout << "Found " << shared->num_exam_files << " exam files\n";
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <number_of_TAs>\n";
        return 1;
    }
    
    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        std::cerr << "Error: Must have at least 2 TAs\n";
        return 1;
    }
    
    std::cout << "=== TA Marking System (Part 2b - WITH semaphores) ===\n";
    std::cout << "Number of TAs: " << num_tas << "\n\n";
    
    srand(time(NULL));
    
    // Create shared memory
    int shm_fd = shm_open("/ta_marking_shm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }
    
    ftruncate(shm_fd, sizeof(SharedData));
    
    SharedData* shared = (SharedData*)mmap(NULL, sizeof(SharedData),
                                           PROT_READ | PROT_WRITE,
                                           MAP_SHARED, shm_fd, 0);
    if (shared == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    
    // Initialize shared memory
    memset(shared, 0, sizeof(SharedData));
    shared->all_done = false;
    shared->total_exams_loaded = 0;
    shared->next_exam_to_load = 0;
    shared->reader_count = 0;
    
    // Initialize semaphores (process-shared)
    sem_init(&shared->rubric_mutex, 1, 1);
    sem_init(&shared->reader_count_mutex, 1, 1);
    sem_init(&shared->exam_load_mutex, 1, 1);
    
    // Load rubric
    std::cout << "Loading rubric into shared memory...\n";
    load_rubric(shared);
    
    // Get list of exam files
    get_exam_files(shared);
    
    // Load first exam
    if (shared->num_exam_files > 0) {
        std::cout << "Loading first exam into shared memory...\n";
        load_exam_into_memory(shared, shared->exam_filenames[0], 0);
        shared->total_exams_loaded = 1;
        shared->next_exam_to_load = 1;
        std::cout << "First exam: Student " << shared->exams[0].student_number << "\n\n";
    } else {
        std::cerr << "Error: No exam files found\n";
        return 1;
    }
    
    // Create TA processes
    std::vector<pid_t> ta_pids;
    for (int i = 0; i < num_tas; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            ta_process(shared, i + 1);
            exit(0);
        } else if (pid > 0) {
            ta_pids.push_back(pid);
        } else {
            perror("fork");
            return 1;
        }
    }
    
    // Wait for all TAs
    for (pid_t pid : ta_pids) {
        waitpid(pid, NULL, 0);
    }
    
    std::cout << "\n=== All TAs finished ===\n";
    std::cout << "Total exams processed: " << shared->total_exams_loaded << "\n";
    
    // Cleanup semaphores
    sem_destroy(&shared->rubric_mutex);
    sem_destroy(&shared->reader_count_mutex);
    sem_destroy(&shared->exam_load_mutex);
    
    for (int i = 0; i < shared->total_exams_loaded; i++) {
        sem_destroy(&shared->exams[i].exam_mutex);
    }
    
    // Cleanup
    munmap(shared, sizeof(SharedData));
    close(shm_fd);
    shm_unlink("/ta_marking_shm");
    
    return 0;
}