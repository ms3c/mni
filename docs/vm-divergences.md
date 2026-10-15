# Differences between the VM and the tree-walker

The bytecode VM is meant to behave exactly like the tree-walking evaluator: same values, same output, same Swahili error text and `Mstari N:` line. A golden corpus (`test/corpus`) and a seeded random program fuzzer (`test/source/differential.cpp`) compare them. There are two intentional differences, both in programs that are already errors.

1. **`vunja`, `endelea` or `rudisha` inside a parameter default.** The tree-walker does not check for a pending flow change while it fills in defaults, so the first statement of the function body still runs before the error appears. The VM raises the error at once.
2. **A stray `vunja` or `endelea` in a function body** (no loop around it). The VM throws at the statement itself, using the line of the call that entered the function. The message and line are the same as the tree-walker's.

The fuzzer keeps both out of its programs.

## Limits

Both backends count ticks in the same places (a completed loop iteration, a user function call). A program stopped by a tick limit is stopped by both, but the exact tick at which each stops is not compared.
