# Building
Linux environment is Required.
Install dependencies
```
sudo apt install git g++ make python-minimal
```

Clone repos
```
git clone https://github.com/ESampler/Esampler.git
git clone https://github.com/Z3Prover/z3.git
```

Build z3
```
cd z3
python scripts/mk_make.py
cd build
make
sudo make install
cd ../..
```

Build baseline ESampler

```
cd baseline
make
```

Build ESampler with QuickSampler as Solution Enumerator

```
cd extended
make
```

# Running

## Baseline

### Algorithm

Most smaplers have complex heuristic which may effect our evalution of the effectiveness of the deverivation procedure, the uniformity of the sampled solutions and the generating speed change over time, therefore, we implement a baseline ESampler with such simple algirithm as the seed enumerator:

randomly assign the variable in the independent support of the given formula and give it to the Max-SAT solver as soft constraints to get a seed solution each time.

We also propose a heuristic method to adjust the number of variables to assign for each Max-SAT round (trade-off between the solver time for each Max-SAT problem and the repeated rate of the seed generated). The heuristic is based on the repeated rate of the recent 100 seeds, and it will control the he number of variables to assign for each Max-SAT to maintain the repeated rate of the seed soutions in a reasonable range.

### Running
```
./esampler -n 1000000 -t 7200.0 -i -r --drv 50 -e formula.cnf
```

Use '-i' to enable the independent support information.

Use '-e' to enable enumerate mode, which only output unique solution.

Use '-n' to set the maximum samples number, default value is 10000000.

Use '-t' to set the time limit, default value is 7200.

Use '-r' to enable the heuristic soft constraint, which will random choose the soft constraint variable by the probility of the repeat rate of samples.

Use '-d' to enable debug mode, which will print debug information on the terminal.

Use '-nd' to disable the derivation procedure, default is enable.

Use '--drv' to set the max derivation limit in one derivation time, default is 10000.

## Extended
### Algorithm
Extended ESampler algirithm using QuickSampler as the seed enumerator.
### Running
```
./extended -n 1000000 -t 7200.0 --drv 50 -e formula.cnf
```
Use '-n' to set the maximum samples number, default value is 10000000.

Use '-t' to set the time limit, default value is 7200.

Use '-d' to enable the derivation procedure, default is disabled.

# Benchmarks
We use Benchmarks from UniGen. Find them [here](https://github.com/meelgroup/sampling-benchmarks/tree/master/unigen-benchmarks).



