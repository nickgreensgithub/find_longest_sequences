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

## Example usage
input_file.fa
```bash
>one
GAT
>two
EGACA
>three
GAAB
>four
GAAT
>five
GAATA
>six
GATACR
>seven
EGATA
>eight
EGATA
>nine
GAACA
>ten
EGATA
```

Running with 1 thread:
```bash
./findLongSeqs ./input_file.fa ./output_file.fa 1

Reading input file...
done
Sorting sequences by length for faster searching...
done
Creating sequence length index...
done
Dereplicating sequences...
done
Original sequence count: 10
{         █          █          █          █    █} 100%
Final sequence count: 6
Writing output file
done
Process took: 0 seconds to complete
```

output_file.fa
```bash
>two
EGACA
>three
GAAB
>five
GAATA
>six
GATACR
>seven
EGATA
>nine
GAACA
```