# Native kernel choice and measured scope

The original 0.4.6.2 synaptic memory mode used C11 for fixed-point neurons and the
five-qubit stabilizer tableau, C++23 for bounded scenario admission and
receipts, and a compiled Fortran 2008 `ISO_C_BINDING` routine for signed
vector reconstruction from recovered logical bits. This is a production
cross-language call in every enabled network tick, not a standalone example.
The archived measured binary contained the Fortran object. In 0.4.7, the
build selects that implementation when a Fortran compiler exists, or the
same complete integer decoder ABI implemented in C11 otherwise. The
two implementations are separate executable backends, not mixed in a run.
The archived Fortran timing results below are historical and do not describe
the selected backend or the new ancilla circuit on a different host.

## Isolated kernel measurement

`make bin/bench_qmemory && bin/bench_qmemory` generates fifteen raw rows with
columns `edges,sample,fortran_ns,scalar_cpp_ns`. Each timed sample performs
500 calls on the same seeded bit vector, after 50 warmup calls. The C++
reference validates each logical bit as zero or one, just as the Fortran
kernel does. Samples alternate execution order. Outputs are checked for exact
equality before and during timing. Both binaries used GCC 13 on this
virtualized x86-64 host. The Fortran object used `-O3 -funroll-loops`
with bounds checks, while the scalar C++ reference used `-O2`.

| Edges | Fortran median ns | Scalar C++ median ns | Scalar / Fortran |
| ---: | ---: | ---: | ---: |
| 64 | 461,495 | 638,142 | 1.38 |
| 256 | 1,710,818 | 2,714,682 | 1.59 |
| 1,024 | 7,142,084 | 10,614,117 | 1.49 |

These are medians of the included raw five samples per size under the recorded
compiler flags. They measure only the logical-bit reconstruction kernel;
they do not establish an end-to-end SNN speedup, multicore scaling, or any
speed ranking against other neural or quantum simulators. Rebuild and rerun
on a target before choosing an alternative kernel there.

## Restore and vector classifier experiments

The restore path now checks each unmodified 120-byte tableau against its
canonical logical basis codeword before entering scalar syndrome correction.
Declared faults still enter `pq_five_correct` and are compared with the full
canonical tableau after recovery. Any undeclared noncanonical tableau is
refused. Signed bit reconstruction still calls the compiled Fortran routine
on every restore. This reduces repeated syndrome work on the common clean
path while strengthening the check on undeclared tableau changes.

The actual `Weights::restore` benchmark (`make bin/bench_qmemory_restore &&
bin/bench_qmemory_restore`) ran ten restorations per timed sample, with five
samples after three warmups. The baseline was the previous unmodified source
on the same host and compiler; the included `RESTORE_BEFORE_RAW.csv` and
`RESTORE_AFTER_RAW.csv` hold the individual times. Medians in nanoseconds for
ten restorations were:

| Edges | Previous restore | Canonical clean path | Previous / current |
| ---: | ---: | ---: | ---: |
| 64 | 555,878 | 139,700 | 3.98 |
| 256 | 1,845,671 | 382,406 | 4.83 |
| 1,024 | 7,259,218 | 1,436,265 | 5.05 |

These are isolated clean restores. They do not include receipt creation,
network transitions, or the cost of many declared faults. The optimized
program passed all fifteen physical Pauli recovery paths and byte-exact
scenario replay after this change.

ISPC 1.22 was compiled for AVX2 and tested against `std::memcmp` on the
real 120-byte `pq_row` tableau layout, using the same canonical codewords.
`make bench-ispc` is an optional reproducible candidate benchmark; it does
not enter the released executable. Five samples of five calls each, in
alternating order, are in `ISPC_TRIAL_RAW.csv` as
`cells,sample,scalar_ns,ispc_ns`. Median times in nanoseconds were:

| Cells | Scalar C++ | ISPC AVX2 | ISPC / scalar |
| ---: | ---: | ---: | ---: |
| 4,096 | 94,542 | 246,422 | 2.61 |
| 32,768 | 820,517 | 2,436,252 | 2.97 |
| 131,072 | 3,526,185 | 13,386,910 | 3.80 |

ISPC warned that the array-of-structs layout requires gather loads across
vector lanes. The measured candidate is slower here, so it is not enabled in
the runtime. The Fortran decoder is retained and independently measured.

In 0.4.6.3, faulted cells additionally use a Fortran entry point that decodes
the four-bit syndrome and raw logical-Z sign and checks a separately
maintained expected bit before weight commit. C++ cross-checks the Pauli
index, logical value and full corrected tableau. Clean restores keep the
previous entry point and a constant-time active-fault flag. The older
clean-path timing table is not a new end-to-end benchmark of this change.
The new isolated clean samples are in
[`READOUT_RESTORE_0463_RAW.csv`](../evidence/qmemory/READOUT_RESTORE_0463_RAW.csv);
no acceleration claim is made from them.

In 0.4.7, the expected-bit argument and retained encoded-word snapshot are
removed from the active decoder. A sixth simulated qubit performs explicit
ancilla syndrome extraction and logical-Z readout on faulted codewords. The
decoder reconstructs a bit from that observation; C++ checks its correction
index and requires a complete canonical tableau before committing weights.
Only weights changed by the network transition are re-encoded, using the
pre-step weight vector already needed for plasticity auditing. The clean
restore benchmark above predates this release; no speedup is claimed for the
new circuit.
Optional `bench-ispc` needs ISPC and an AVX2-capable x86-64 CPU; ordinary
builds do not require either.

A second compiled ISPC kernel uses 30 contiguous word planes, eliminating
gathers. `ISPC_SOA_RAW.csv` records the scalar comparison, SIMD
classification and the required transpose as separate columns. Median times
for five calls, in the same cell order, were:

| Cells | Scalar C++ ns | Contiguous ISPC ns | Transpose ns |
| ---: | ---: | ---: | ---: |
| 4,096 | 95,384 | 274,033 | 405,711 |
| 32,768 | 790,502 | 2,525,546 | 6,101,255 |
| 131,072 | 3,189,447 | 11,093,329 | 32,911,271 |

The transposition is measured separately and is needed whenever the canonical
tableaux change. Even if planes were maintained continuously, classification
alone lost here. Both ISPC sources and their exact C++ comparison drivers are
included as optional build targets. No ISPC path is claimed to accelerate the
ordinary executable on this host.

## Evaluated integrations

| Candidate | Relevant fit | Release decision |
| --- | --- | --- |
| Fortran 2008 | Flat numerical arrays with a standardized C ABI | Compiled and measured in the executable restore path |
| Ada / SPARK | Possible bounded contracts for a separate decoder | No GNATprove run or formal proof is claimed; a type declaration cannot establish correctness of the whole quantum runtime |
| Common Lisp / SBCL | Symbolic rule generation | Not put into a latency-sensitive tick without a measured executable and validated ABI |
| C89 | Portable scalar bit routines | Reverting the C11 kernel alone provides no measured vectorization gain |
| Julia PackageCompiler | Dense tensor or matrix work with a stable C ABI | The current five-qubit path is sparse integer/tableau work. A generated library includes Julia runtime initialization and dependencies; no local toolchain or measured gain here |
| Halide | Multi-stage dense array or image bitplane pipelines | No installed generator or suitable dense pipeline in this bounded restore; replacing the branchy correction path lacks a demonstrated gain |
| Chapel | Array and distributed computation | Library integration needs runtime initialization; no installed compiler or applicable measured scale here |
| ISPC | SIMD batches of independent tableau classification | Both array-of-structs and contiguous-plane AVX2 kernels built and validated; each measured slower than scalar C++ here; optional benchmarks, not the runtime |

The evaluations follow official documentation for [Fortran C interoperability](https://gcc.gnu.org/onlinedocs/gfortran/Mixed-Language-Programming.html), [SPARK proof limitations](https://docs.adacore.com/spark2014-docs/html/ug/en/appendix/gnatprove_limitations.html), [Julia library generation](https://julialang.github.io/PackageCompiler.jl/stable/refs.html), [Halide ahead-of-time scheduling](https://halide-lang.org/docs/tutorial/lesson_10_aot_compilation_generate.html), [Chapel C libraries](https://chapel-lang.org/docs/technotes/libraries.html) and the [ISPC user guide](https://ispc.github.io/ispc.html). Neither a different language nor SIMD guarantees faster or more correct execution without a tested workload and invariant.
