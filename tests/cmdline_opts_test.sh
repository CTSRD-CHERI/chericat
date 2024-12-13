#!/bin/sh

bindir="../bin"
bin=$bindir"/chericat"

########
# Test that chericat with an invalid argument would output an error message
########
pass=0
output=$($bin -z 2>&1)
echo "$output" | grep -q 'invalid option' -

if [ $? == 0 ]; then
    pass=1
else 
    echo "Unexpected result for passing in an invalid option"
    exit 1
fi

########
# Test that chericat without input would output usage text
########
pass=0
output=$($bin 2>&1)
echo "$output" | grep -q 'Usage' -

if [ $? == 0 ]; then
    pass=1
else 
    echo "Unexpected result for no input"
    exit 1
fi

########
# Test that chericat with -f without a filename would result in an error message
########
pass=0
output=$($bin -f 2>&1)
echo "$output" | grep -q 'requires an argument' -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -f with no input"
    exit 1
fi

########
# Test that chericat with -p without a pid would result in an error message
########
pass=0
output=$($bin -p 2>&1)
echo "$output" | grep -q 'requires an argument' -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -p with no input"
    exit 1
fi

########
# Test that chericat with -v without -l or -c would result in an error message
########
pass=0
output=$($bin -v 2>&1)
echo "$output" | grep -q "\-v requires either \-l (library view) or \-c (compartment view)" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -v without specifying -l or -c"
    exit 1
fi

########
# Test that chericat with -lv without -f or -p option would result in an error message
########
pass=0
output=$($bin -lv 2>&1)
echo "$output" | grep -q "vm table does not exist on db" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -lv without -f or -p"
    exit 1
fi

########
# Test that chericat with -i <name> without -l or -c would result in an error message
########
pass=0
output=$($bin -i "somename" 2>&1)
echo "$output" | grep -q "\-i requires either \-l (library view) or \-c (compartment view)" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -i <name> without specifying -l or -c"
    exit 1
fi

########
# Test that chericat with -li without the library name would result in an error message
########
pass=0
output=$($bin -li 2>&1)
echo "$output" | grep -q "requires an argument" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -li with no input"
    exit 1
fi

########
# Test that chericat with -li <library name> without -f or -p option would result in an error message
########
pass=0
output=$($bin -li some_library 2>&1)
echo "$output" | grep -q "cap_info table does not exist on db" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -li <library name> without -f or -p"
    exit 1
fi

########
# Test that chericat with -ci without the compartment name would result in an error message
########
pass=0
output=$($bin -ci 2>&1)
echo "$output" | grep -q "requires an argument" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -ci with no input"
    exit 1
fi

########
# Test that chericat with -p with an invalid pid would result in an error message
########
pass=0
output=$($bin -p 123 2>&1)
echo "$output" | grep -q "No such process" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -p with an invalid pid"
    exit 1
fi

########
# Test that chericat with -f with an invalid database name together with -v option would result in an error message
########
pass=0
output=$($bin -f invalid -lv 2>&1)
echo "$output" | grep -q "vm table does not exist" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -f with an invalid database name, along with the -v option"
    exit 1
fi

########
# Test that chericat with -f with an invalid database name together with -i option would result in an error message
########
pass=0
output=$($bin -f invalid -li some_library 2>&1)
echo "$output" | grep -q "cap_info table does not exist" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -f with an invalid database name, along with the -i option"
    exit 1
fi

########
# Check overall test status
#########
if [ $pass == 1 ]; then
    echo "Test OK!"
else
    echo "Test FAIL"
fi

########
# Clean up
#########
rm invalid

exit 0
