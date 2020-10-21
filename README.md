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

## Baseline:

```
./esampler -n 1000000 -t 7200.0 -i -r --drv 50 -e formula.cnf
```

Use '-i' to enable the independent support information.

Use '-e' to enable enumerate mode, which only output unique solution.

Use '-n' to set the maximum samples number, default value is 10000000.

Use '-t' to set the time limit, default value is 7200.

Use '-r' to enable the heuristic soft constraint choosing F-strategy, which will random choose the soft constraint variable by the probility of the repeat rate of samples.

Use '-d' to enable debug mode, which will print debug information on the terminal.

Use '-nd' to disable the derivation procedure, default is enable.

Use '--drv' to set the max derivation limit in one derivation time, default is 10000.

## Extended:

```
./extended -n 1000000 -t 7200.0 --drv 50 -e formula.cnf
```
Use '-n' to set the maximum samples number, default value is 10000000.

Use '-t' to set the time limit, default value is 7200.

Use '-d' to enable the derivation procedure, default is disabled.

# Benchmarks
We use Benchmarks from UniGen. Find them [here](https://github.com/meelgroup/sampling-benchmarks/tree/master/unigen-benchmarks).

# Appendix of Paper

## Algorithm Speed 

## Uniformity Test

## Heuristic 

## Comparision

<object data="figs/E_U.pdf" type="application/pdf" width="700px" height="700px">
    <embed src="figs/E_U.pdf">
        <p>This browser does not support PDFs. Please download the PDF to view it: <a href="figs/E_U.pdf">Download PDF</a>.</p>
    </embed>
</object>



