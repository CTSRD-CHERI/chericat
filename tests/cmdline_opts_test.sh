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
# Test that chericat with -v without show lib or show comp command would result in an error message
########
pass=0
output=$($bin -v 2>&1)
echo "$output" | grep -q "Expecting \"show lib|comp\" command after the options" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -v without specifying show lib or show comp commands"
    exit 1
fi

########
# Test that chericat with -v without -f or -p option would result in an error message
########
pass=0
output=$($bin -v show lib 2>&1)
echo "$output" | grep -q "vm table does not exist on db" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -v show lib without -f or -p"
    exit 1
fi

########
# Test that chericat with -i <name> without show lib or show comp command would result in an error message
########
pass=0
output=$($bin -i "somename" 2>&1)
echo "$output" | grep -q "Expecting \"show lib|comp\" command after the options" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -i <name> without specifying show lib or show comp"
    exit 1
fi

########
# Test that chericat with -i without the library or compartment name would result in an error message
########
pass=0
output=$($bin -i 2>&1)
echo "$output" | grep -q "requires an argument" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -i with no input"
    exit 1
fi

########
# Test that chericat with -i <library name> show lib|comp without -f or -p option would result in an error message
########
pass=0
output=$($bin -i some_library show lib 2>&1)
echo "$output" | grep -q "cap_info table does not exist on db" -
if [ $? == 0 ]; then 
    pass=1
else
    echo "Unexpected result for -i <library name> without -f or -p"
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
output=$($bin -f invalid -v show lib 2>&1)
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
output=$($bin -f invalid -i some_library show lib 2>&1)
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
