# Find longest sequences
A simple tool written in c++ for use in [AutoTax](https://github.com/kasperskytte/autotax), it dereplicates and filters sequences in an input fasta file. Sequences are removed if longer sequences are found which are 100% identical, relative order of sequences is maintained.

## Compilation:
```bash
cmake .
make
```
## Usage: 
```bash
findLongSeqs <input_file> <output_file> <threads>
```
## Dependencies:
- OpenMP