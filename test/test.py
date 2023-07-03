import subprocess
from pathlib import Path

scripts = Path(".").glob("*.dpl")


def get_tests_in_file(path):
    tests = []
    text = path.read_text(encoding="utf-8")
    description = ""
    for line in text.splitlines():
        if line.startswith("// [TEST] "):
            description = line[10:]
        expectation = line.find("// expect: ")
        if expectation != -1:
            statement = line[:expectation]
            expected_value = line[expectation + 11 :]
            tests.append((description, statement, expected_value))
    return tests


def run_test(path):
    p = subprocess.run(
        ["../build/dplang", path],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    all_output = p.stdout.splitlines()
    tests = get_tests_in_file(path)
    pdesc = None
    for output, (description, statement, expected_output) in zip(all_output, tests):
        if description != pdesc:
            print(f"  === {description}")
            pdesc = description
        result = "OK" if output == expected_output else "FAIL"
        print(f"   {statement} [{result}]")


for script in scripts:
    print(f"=== {script}")
    run_test(script)
