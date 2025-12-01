#!/bin/bash
# setup_test_files.sh
# Creates the rubric and exam files needed for Part 2

echo "Creating rubric.txt..."
cat > rubric.txt << 'EOF'
1, A
2, B
3, C
4, D
5, E
EOF

echo "Creating exam files..."

# Create 20+ exam files with student numbers
for i in {1..25}; do
    student_num=$(printf "%04d" $i)
    filename="exam_${student_num}.txt"
    
    echo "Creating $filename..."
    cat > $filename << EOF
$student_num
Question 1: Student $student_num answer for Q1
Question 2: Student $student_num answer for Q2
Question 3: Student $student_num answer for Q3
Question 4: Student $student_num answer for Q4
Question 5: Student $student_num answer for Q5
EOF
done

# Create the termination file (student 9999)
echo "Creating exam_9999.txt (termination file)..."
cat > exam_9999.txt << 'EOF'
9999
Question 1: Final exam
Question 2: Final exam
Question 3: Final exam
Question 4: Final exam
Question 5: Final exam
EOF

echo ""
echo "=== Setup Complete ==="
echo "Created:"
echo "  - rubric.txt (5 rubric entries)"
echo "  - exam_0001.txt through exam_0025.txt (25 exams)"
echo "  - exam_9999.txt (termination exam)"
echo ""
echo "Total: 27 files"
echo ""
echo "You can now compile and run the programs:"
echo "  make all"
echo "  ./ta_marking_2a 3"
echo "  ./ta_marking_2b 3"