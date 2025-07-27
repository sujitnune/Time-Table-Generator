#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For getcwd()
#include <time.h>   // For random seed

#define MAX_SUBJECTS 50
#define MAX_FACULTIES 100
#define MAX_NAME_LEN 100
#define MAX_SECTIONS 3
#define MAX_DAYS 6
#define MAX_SLOTS_PER_DAY 7 // 9:40 to 4:20 with 1-hour slots & 1 lunch break
#define MAX_TOTAL_SLOTS (MAX_DAYS * MAX_SLOTS_PER_DAY)
#define LUNCH_SLOT 3 // 4th slot is lunch break

typedef struct {
    char name[MAX_NAME_LEN];
    int duration;
    int facultyCount;
    char faculties[MAX_FACULTIES][MAX_NAME_LEN];
    int assigned[MAX_SECTIONS]; // Track assignment status per section
    int occurrences;  // How many times this subject should appear per week
    int section_specific; // If 1, this subject is tied to a specific section
    int target_section;   // Which section this subject is for (if section_specific is 1)
} Subject;

Subject subjects[MAX_SUBJECTS];
int subjectCount = 0;

// The timetable for all sections
int timetable[MAX_SECTIONS][MAX_DAYS][MAX_SLOTS_PER_DAY];

// Faculty availability tracking
int facultyBusy[MAX_FACULTIES][MAX_DAYS][MAX_SLOTS_PER_DAY];
int totalFaculties = 0;
char allFaculties[MAX_FACULTIES][MAX_NAME_LEN];

// Find or add faculty
int getFacultyIndex(const char *name) {
    // Skip empty names
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    
    for (int i = 0; i < totalFaculties; i++) {
        if (strcmp(allFaculties[i], name) == 0) {
            return i;
        }
    }
    
    // Not found, add new faculty
    if (totalFaculties < MAX_FACULTIES) {
        strcpy(allFaculties[totalFaculties], name);
        return totalFaculties++;
    }
    
    // Too many faculties, return error
    printf("ERROR: Maximum faculty limit reached!\n");
    return -1;
}

// Check if faculty is available
int isFacultyAvailable(const char *name, int day, int slot) {
    // Skip empty names
    if (name == NULL || name[0] == '\0') {
        return 1;  // Empty faculty is always available
    }
    
    int idx = getFacultyIndex(name);
    if (idx == -1) return 1;  // If faculty not found, assume available
    
    return !facultyBusy[idx][day][slot];
}

// Mark faculty as busy
void markFacultyBusy(const char *name, int day, int slot) {
    // Skip empty names
    if (name == NULL || name[0] == '\0') {
        return;
    }
    
    int idx = getFacultyIndex(name);
    if (idx == -1) return;  // Skip if faculty index invalid
    
    facultyBusy[idx][day][slot] = 1;
}

// Set default occurrences for subjects based on their type and name
void setDefaultOccurrences(Subject *subject) {
    // Reset default value
    subject->occurrences = 1;
    
    // Check if this is a lab subject (2-hour duration)
    if (subject->duration == 2) {
        // Labs typically appear once per week
        subject->occurrences = 1;
    } else {
        // Theory subjects appear multiple times per week
        if (strstr(subject->name, "HVPE") != NULL || 
            strcmp(subject->name, "Sports") == 0 || 
            strcmp(subject->name, "Library") == 0 ||
            strcmp(subject->name, "Mentoring") == 0) {
            subject->occurrences = 1;
        } else if (strcmp(subject->name, "OE-II") == 0) {
            subject->occurrences = 2;  // Based on the timetable image showing OE-II twice a week
        } else {
            // Regular theory subjects appear 3 times per week
            subject->occurrences = 3;
        }
    }
    
    printf("Set occurrences for %s: %d\n", subject->name, subject->occurrences);
}

void readSubjectsFromFile(const char *filename) {
    printf("Attempting to open file: %s\n", filename);
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Cannot open file: %s\n", filename);
        printf("Try using full path or check if file exists in current directory.\n");
        exit(1);
    }
    
    printf("File opened successfully.\n");
    
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline character
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        printf("Processing line: %s\n", line);

        char *token = strtok(line, ",");
        if (!token) {
            printf("Warning: Invalid line format (no subject name)\n");
            continue;
        }
        
        strcpy(subjects[subjectCount].name, token);
        printf("Subject name: %s\n", subjects[subjectCount].name);

        token = strtok(NULL, ",");
        if (!token) {
            printf("Warning: Invalid line format (no duration) for subject %s\n", subjects[subjectCount].name);
            continue;
        }
        
        subjects[subjectCount].duration = atoi(token);
        printf("Duration: %d\n", subjects[subjectCount].duration);

        subjects[subjectCount].facultyCount = 0;
        token = strtok(NULL, "\n");
        if (token && token[0] != '\0') {
            char facultyCopy[512];
            strcpy(facultyCopy, token);
            
            char *faculty = strtok(facultyCopy, ";");
            while (faculty && subjects[subjectCount].facultyCount < MAX_FACULTIES) {
                strcpy(subjects[subjectCount].faculties[subjects[subjectCount].facultyCount++], faculty);
                printf("Faculty %d: %s\n", subjects[subjectCount].facultyCount, faculty);
                faculty = strtok(NULL, ";");
            }
        } else {
            printf("No faculties for this subject\n");
        }

        // Initialize assignment status for all sections
        for (int s = 0; s < MAX_SECTIONS; s++) {
            subjects[subjectCount].assigned[s] = 0;
        }
        
        // By default, subject is not section-specific
        subjects[subjectCount].section_specific = 0;
        subjects[subjectCount].target_section = -1;
        
        // Set default occurrences based on subject type
        setDefaultOccurrences(&subjects[subjectCount]);
        
        subjectCount++;
        
        if (subjectCount >= MAX_SUBJECTS) {
            printf("Warning: Maximum subject limit reached. Some subjects may be ignored.\n");
            break;
        }
    }

    fclose(fp);
    printf("Total subjects read: %d\n", subjectCount);
}

// Check if a subject can be placed at a specific day and time slot
int canPlaceSubject(int subIdx, int day, int slot, int section) {
    // Check if subject is section-specific but not for this section
    if (subjects[subIdx].section_specific && 
        subjects[subIdx].target_section != section) {
        return 0;
    }
    
    // Check if the slot is already occupied
    if (timetable[section][day][slot] != -1) {
        return 0;
    }
    
    // Check if slot is lunch break
    if (slot == LUNCH_SLOT) {
        return 0;
    }
    
    // For 2-hour subjects, check if we have two consecutive slots available
    if (subjects[subIdx].duration == 2) {
        // Can't place 2-hour subject in last slot of the day
        if (slot == MAX_SLOTS_PER_DAY - 1) {
            return 0;
        }
        
        // Check if next slot is available and not lunch
        if (slot + 1 == LUNCH_SLOT || timetable[section][day][slot + 1] != -1) {
            return 0;
        }
    }
    
    // Check faculty availability
    for (int f = 0; f < subjects[subIdx].facultyCount; f++) {
        if (!isFacultyAvailable(subjects[subIdx].faculties[f], day, slot)) {
            return 0;
        }
        
        // For 2-hour subjects, check faculty availability for next hour
        if (subjects[subIdx].duration == 2 && slot + 1 < MAX_SLOTS_PER_DAY) {
            if (!isFacultyAvailable(subjects[subIdx].faculties[f], day, slot + 1)) {
                return 0;
            }
        }
    }
    
    // Check if this subject has already been scheduled on this day
    for (int s = 0; s < MAX_SLOTS_PER_DAY; s++) {
        if (timetable[section][day][s] == subIdx) {
            return 0;
        }
    }
    
    return 1;
}

// Count how many times a subject appears in a section's timetable
int countSubjectOccurrences(int subIdx, int section) {
    int count = 0;
    for (int d = 0; d < MAX_DAYS; d++) {
        for (int s = 0; s < MAX_SLOTS_PER_DAY; s++) {
            if (timetable[section][d][s] == subIdx) {
                // For 2-hour subjects, only count once per occurrence
                if (s == 0 || timetable[section][d][s-1] != subIdx) {
                    count++;
                }
            }
        }
    }
    return count;
}

// Place subject in timetable and mark faculty as busy
void placeSubject(int subIdx, int day, int slot, int section, int occurrence) {
    printf("Placing subject %s on day %d, slot %d for section %d (occurrence %d)\n", 
           subjects[subIdx].name, day, slot, section, occurrence);
           
    timetable[section][day][slot] = subIdx;
    
    // For 2-hour subjects, occupy two slots
    if (subjects[subIdx].duration == 2 && slot + 1 < MAX_SLOTS_PER_DAY) {
        timetable[section][day][slot + 1] = subIdx;
    }
    
    // Mark faculty as busy
    for (int f = 0; f < subjects[subIdx].facultyCount; f++) {
        markFacultyBusy(subjects[subIdx].faculties[f], day, slot);
        
        // For 2-hour subjects
        if (subjects[subIdx].duration == 2 && slot + 1 < MAX_SLOTS_PER_DAY) {
            markFacultyBusy(subjects[subIdx].faculties[f], day, slot + 1);
        }
    }
    
    // Mark as fully assigned if all occurrences are scheduled
    if (occurrence >= subjects[subIdx].occurrences) {
        subjects[subIdx].assigned[section] = 1;
    }
}

// Try to place a specific subject in the timetable
// Returns 1 if successful, 0 if failed
int tryPlaceSubject(int subIdx, int section) {
    int occurCount = 0;
    
    while (occurCount < subjects[subIdx].occurrences) {
        int placed = 0;
        
        // Try different days and slots randomly to increase variety
        int days[MAX_DAYS];
        int slots[MAX_SLOTS_PER_DAY];
        
        // Initialize the arrays
        for (int i = 0; i < MAX_DAYS; i++) days[i] = i;
        for (int i = 0; i < MAX_SLOTS_PER_DAY; i++) slots[i] = i;
        
        // Shuffle days
        for (int i = MAX_DAYS - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            int temp = days[i];
            days[i] = days[j];
            days[j] = temp;
        }
        
        // Shuffle slots
        for (int i = MAX_SLOTS_PER_DAY - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            int temp = slots[i];
            slots[i] = slots[j];
            slots[j] = temp;
        }
        
        // Try each day and slot in random order
        for (int di = 0; di < MAX_DAYS && !placed; di++) {
            int d = days[di];
            for (int si = 0; si < MAX_SLOTS_PER_DAY && !placed; si++) {
                int s = slots[si];
                
                if (canPlaceSubject(subIdx, d, s, section)) {
                    placeSubject(subIdx, d, s, section, occurCount + 1);
                    placed = 1;
                    occurCount++;
                }
            }
        }
        
        if (!placed) {
            printf("WARNING: Could not place subject: %s occurrence %d for section %d\n", 
                   subjects[subIdx].name, occurCount + 1, section + 1);
            return 0;
        }
    }
    
    return 1;
}

// Generate timetable using an improved approach with better distribution
void generateTimetable() {
    printf("Starting timetable generation...\n");
    
    // Seed the random number generator for shuffling
    srand(time(NULL));
    
    // Initialize timetable with -1 (free)
    for (int s = 0; s < MAX_SECTIONS; s++) {
        for (int d = 0; d < MAX_DAYS; d++) {
            for (int p = 0; p < MAX_SLOTS_PER_DAY; p++) {
                timetable[s][d][p] = -1;
            }
        }
    }
    
    // Initialize faculty availability
    for (int f = 0; f < MAX_FACULTIES; f++) {
        for (int d = 0; d < MAX_DAYS; d++) {
            for (int p = 0; p < MAX_SLOTS_PER_DAY; p++) {
                facultyBusy[f][d][p] = 0;
            }
        }
    }
    
    // Create a priority queue of subjects (prioritize 2-hour subjects and higher occurrence counts)
    int priorityOrder[MAX_SUBJECTS];
    
    // Initialize the priority order
    for (int i = 0; i < subjectCount; i++) {
        priorityOrder[i] = i;
    }
    
    // Sort by priority (2-hour subjects first, then by occurrence count)
    for (int i = 0; i < subjectCount - 1; i++) {
        for (int j = i + 1; j < subjectCount; j++) {
            int iIdx = priorityOrder[i];
            int jIdx = priorityOrder[j];
            
            // Higher duration gets higher priority
            if (subjects[jIdx].duration > subjects[iIdx].duration) {
                int temp = priorityOrder[i];
                priorityOrder[i] = priorityOrder[j];
                priorityOrder[j] = temp;
            }
            // If same duration, higher occurrence count gets higher priority
            else if (subjects[jIdx].duration == subjects[iIdx].duration && 
                     subjects[jIdx].occurrences > subjects[iIdx].occurrences) {
                int temp = priorityOrder[i];
                priorityOrder[i] = priorityOrder[j];
                priorityOrder[j] = temp;
            }
        }
    }
    
    printf("Placement order of subjects:\n");
    for (int i = 0; i < subjectCount; i++) {
        int subIdx = priorityOrder[i];
        printf("%d. %s (Duration: %d, Occurrences: %d)\n", 
               i+1, subjects[subIdx].name, subjects[subIdx].duration, subjects[subIdx].occurrences);
    }
    
    // Use the priority order to place subjects
    for (int i = 0; i < subjectCount; i++) {
        int subIdx = priorityOrder[i];
        
        for (int s = 0; s < MAX_SECTIONS; s++) {
            if (subjects[subIdx].section_specific && subjects[subIdx].target_section != s) {
                // Skip this section if the subject is not for it
                continue;
            }
            
            printf("Trying to place subject: %s for section %d\n", subjects[subIdx].name, s+1);
            
            if (!tryPlaceSubject(subIdx, s)) {
                printf("WARNING: Failed to place subject %s for section %d. Backtracking needed.\n", 
                       subjects[subIdx].name, s+1);
                // In a more sophisticated version, we could implement backtracking here
            }
        }
    }
    
    // Check for empty slots and try to fill them with additional subjects
    for (int s = 0; s < MAX_SECTIONS; s++) {
        for (int d = 0; d < MAX_DAYS; d++) {
            for (int p = 0; p < MAX_SLOTS_PER_DAY; p++) {
                if (p != LUNCH_SLOT && timetable[s][d][p] == -1) {
                    // Try to find a subject to place here
                    for (int subIdx = 0; subIdx < subjectCount; subIdx++) {
                        // Skip 2-hour subjects for simplicity
                        if (subjects[subIdx].duration == 1 && canPlaceSubject(subIdx, d, p, s)) {
                            // Place an extra occurrence of this subject
                            int currOccur = countSubjectOccurrences(subIdx, s) + 1;
                            placeSubject(subIdx, d, p, s, currOccur);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    printf("Timetable generation completed.\n");
}

void printTimetablesToFiles() {
    printf("Generating timetable output files...\n");
    
    const char *timeSlots[MAX_SLOTS_PER_DAY] = {
        "9:40-10:40", "10:40-11:40", "11:40-12:40", 
        "12:40-1:20 (Lunch)", "1:20-2:20", "2:20-3:20", "3:20-4:20"
    };
    
    const char *days[MAX_DAYS] = {
        "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };

    const char *sectionNames[MAX_SECTIONS] = {
        "IT-A", "IT-B", "IT-C"
    };

    for (int sec = 0; sec < MAX_SECTIONS; sec++) {
        char filename[50];
        sprintf(filename, "section%s_timetable.txt", sectionNames[sec]);
        printf("Creating file: %s\n", filename);
        
        FILE *fp = fopen(filename, "w");
        if (!fp) {
            printf("Error writing to file %s\n", filename);
            printf("Check if you have write permissions in this directory.\n");
            continue;
        }

        fprintf(fp, "=========== VASAVI COLLEGE OF ENGINEERING (AUTONOMOUS) ===========\n");
        fprintf(fp, "=========== DEPARTMENT OF INFORMATION TECHNOLOGY ===========\n");
        fprintf(fp, "=========== TIMETABLE - 2024-25 EVEN SEMESTER ===========\n");
        fprintf(fp, "COURSE: B.E\t\tSEMESTER: IV\t\tBranch/Section: %s\t\tROOM NO.: VS-%d01\n\n", 
                sectionNames[sec], sec+2);

        // Print header
        fprintf(fp, "%-12s", "DAY/TIME");
        for (int s = 0; s < MAX_SLOTS_PER_DAY; s++) {
            fprintf(fp, "%-15s", timeSlots[s]);
        }
        fprintf(fp, "\n");
        
        // Print separator line
        for (int i = 0; i < 12 + 15 * MAX_SLOTS_PER_DAY; i++) {
            fprintf(fp, "-");
        }
        fprintf(fp, "\n");
        
        // Print rows
        for (int d = 0; d < MAX_DAYS; d++) {
            fprintf(fp, "%-12s", days[d]);
            
            for (int s = 0; s < MAX_SLOTS_PER_DAY; s++) {
                if (s == LUNCH_SLOT) { // Lunch break
                    fprintf(fp, "%-15s", "LUNCH");
                } else {
                    int subIdx = timetable[sec][d][s];
                    
                    if (subIdx >= 0 && subIdx < subjectCount) {
                        // Check for continued 2-hour subjects
                        if (s > 0 && timetable[sec][d][s] == timetable[sec][d][s-1] && 
                            subjects[subIdx].duration == 2) {
                            fprintf(fp, "%-15s", "(continued)");
                        } else {
                            fprintf(fp, "%-15s", subjects[subIdx].name);
                        }
                    } else {
                        fprintf(fp, "%-15s", "FREE");
                    }
                }
            }
            fprintf(fp, "\n");
        }

        // Print subject details
        fprintf(fp, "\n\n=========== SUBJECT DETAILS ===========\n\n");
        for (int i = 0; i < subjectCount; i++) {
            fprintf(fp, "Subject: %s\n", subjects[i].name);
            fprintf(fp, "Duration: %d hour(s)\n", subjects[i].duration);
            fprintf(fp, "Weekly Occurrences: %d\n", subjects[i].occurrences);
            fprintf(fp, "Faculty: ");
            if (subjects[i].facultyCount == 0) {
                fprintf(fp, "None assigned");
            } else {
                for (int f = 0; f < subjects[i].facultyCount; f++) {
                    fprintf(fp, "%s", subjects[i].faculties[f]);
                    if (f < subjects[i].facultyCount - 1) fprintf(fp, ", ");
                }
            }
            fprintf(fp, "\n\n");
        }

        fclose(fp);
        printf("Successfully generated timetable for Section %s\n", sectionNames[sec]);
    }
    
    // Generate a faculty workload report
    FILE *fpFaculty = fopen("faculty_workload.txt", "w");
    if (fpFaculty) {
        fprintf(fpFaculty, "=========== FACULTY WORKLOAD REPORT ===========\n\n");
        
        for (int f = 0; f < totalFaculties; f++) {
            int totalHours = 0;
            fprintf(fpFaculty, "Faculty: %s\n", allFaculties[f]);
            fprintf(fpFaculty, "Schedule:\n");
            
            for (int d = 0; d < MAX_DAYS; d++) {
                for (int s = 0; s < MAX_SLOTS_PER_DAY; s++) {
                    if (s == LUNCH_SLOT) continue; // Skip lunch break
                    
                    for (int sec = 0; sec < MAX_SECTIONS; sec++) {
                        int subIdx = timetable[sec][d][s];
                        if (subIdx >= 0) {
                            for (int i = 0; i < subjects[subIdx].facultyCount; i++) {
                                if (strcmp(subjects[subIdx].faculties[i], allFaculties[f]) == 0) {
                                    // Don't double-count continued 2-hour slots
                                    if (!(s > 0 && timetable[sec][d][s] == timetable[sec][d][s-1] && 
                                        subjects[subIdx].duration == 2)) {
                                        fprintf(fpFaculty, "  %s, %s, Section %s, %s\n", 
                                                days[d], timeSlots[s], sectionNames[sec], subjects[subIdx].name);
                                        totalHours++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            fprintf(fpFaculty, "Total Hours: %d\n\n", totalHours);
        }
        
        fclose(fpFaculty);
        printf("Faculty workload report generated.\n");
    }
    
    printf("All timetable files have been generated.\n");
}

int main() {
    printf("Timetable Generator starting...\n");
    
    // Read subjects - try different possible filenames
    const char* filenames[] = {
        "Subjects.txt",
        "subjects.txt",
        "Subjects .txt", // Note the space
        "../Subjects.txt",
        "./Subjects.txt"
    };
    
    int file_found = 0;
    for (int i = 0; i < 5 && !file_found; i++) {
        FILE *test = fopen(filenames[i], "r");
        if (test) {
            fclose(test);
            printf("Found file: %s\n", filenames[i]);
            readSubjectsFromFile(filenames[i]);
            file_found = 1;
        }
    }
    
    if (!file_found) {
        printf("\nERROR: Could not find Subjects.txt file!\n");
        printf("Please ensure the file exists in the current directory.\n");
        printf("Current working directory is: ");
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            printf("unknown\n");
        }
        return 1;
    }
    
    if (subjectCount == 0) {
        printf("No subjects were read from the file. Exiting.\n");
        return 1;
    }
    
    printf("Successfully read %d subjects from file\n", subjectCount);
    
    // Generate timetable
    generateTimetable();
    
    // Generate output files
    printTimetablesToFiles();
    
    printf("Timetable generation completed successfully!\n");
    
    return 0;
}