# Research notes and implementation choices

The distinctive addition in 0.4.1 is an executable experiment-to-protocol path tied to the existing causal receipts: finite perturbation search, measured nonlinear interactions, explicit monotonicity witnesses, deterministic current/latency selection, generated scenario and replayed visualizations. The combination is new to this project. Neither scientific priority nor superiority to Brian/NEST is established by these tests.

Primary work consulted or retrieved:

| Work | Relevance and boundary |
| --- | --- |
| C. A. R. Hoare, *An axiomatic basis for computer programming* (1969), [Oxford author record](https://www.cs.ox.ac.uk/publications/publication8205-abstract.html) | Explicit preconditions and postconditions inform the input and output contracts. This release is tested executable software, not a mechanized Hoare proof. |
| J. J. Hopfield, *Neural networks and physical systems with emergent collective computational abilities* (1982), [Caltech author archive](https://authors.library.caltech.edu/records/w41x7-8bn13) | Historically important state dynamics and content-addressable computation. No Hopfield energy law or convergence guarantee is imported into this SNN. |
| Markram et al., *Regulation of synaptic efficacy by coincidence of postsynaptic APs and EPSPs* (1997), [primary abstract](https://pubmed.ncbi.nlm.nih.gov/8985014/) | Timing-dependent synaptic change motivates temporal observables. The original NeuroMantle linear integer STDP rule is retained, not claimed to reproduce that experiment. |
| Wu et al., *Nonmonotonic Synaptic Excitation and Imbalanced Inhibition Underlying Cortical Intensity Tuning* (2006), [primary abstract](https://pubmed.ncbi.nlm.nih.gov/17114053/) | Shows why a general monotone-response assumption is unsafe. Our explicit refractory example has a different mechanism and is independently tested. |
| Fakhar and Hilgetag, *Systematic perturbation of an artificial neural network* (2022), [open PLOS article](https://journals.plos.org/ploscompbiol/article?id=10.1371/journal.pcbi.1010250) | Demonstrates limitations of interpreting single-site perturbations in interacting networks. Our pair-count mixed difference is narrower than their multi-perturbation lesion analysis and Shapley method. |
| [Brian 2 recording documentation](https://brian2.readthedocs.io/en/latest/user/recording.html) | Spike and state observables are established simulation tools. This release binds its measurements, generated protocols and graphs to repeat execution. |

The full PLOS article and official recording/timing documentation were accessible. Some historical works were available through author records or primary abstracts only; this file does not claim exhaustive reading of six decades of literature. No external research code was copied. The scientific choices are executable and falsifiable on the included scenarios, with an analytical oracle for the refractory example and a separate pair-only success case.
