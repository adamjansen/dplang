#!/bin/sh

function missing_tests() {
    TEST_FILE=${1:?"You must supply a C file with Unity tests"}

    rg '^void test_' ${TEST_FILE} | sed 's/void //g' | sed 's/(void)//g' | sort > tests-defined.txt

    rg 'RUN_TEST' ${TEST_FILE} | sed 's/^.*RUN_TEST(//' | sed 's/);$//g' | sort > tests-run.txt

    MISSING_TESTS=$(sort tests-defined.txt tests-run.txt | uniq -u)

    rm tests-defined.txt tests-run.txt
    echo ${MISSING_TESTS}
}

FAIL=0

if [ "$1" != "" ]; then
    D=$1
else
    D="test"
fi


FILES=$(find ${D} -type f -name 'test_*.c')
for FILE in ${FILES}; do
    echo "Checking ${FILE}"
    MISSING=$(missing_tests ${FILE})
    if [ "${MISSING}" != "" ] ; then
        FAIL=1
        for T in ${MISSING}; do
            echo "   ${T} is not executed"
        done
    fi
done


if [ $FAIL -eq 1 ]; then
    exit 1;
else
    exit 0;
fi
