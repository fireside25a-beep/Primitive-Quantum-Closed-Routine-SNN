# Five-qubit certificate gate execution

`examples/qecc/five_qubit.sqecc` is the certificate checked by the bundled
Surreal verifier. `CERTIFICATE_LOG.txt` records its `[[5,1,3]]` stabilizer
result and code root. `admitted.receipt` is the real three-tick SNN run with
the certificate gate and classical SECDED checkpoint enabled;
`ADMISSION_LOG.txt` and `REPLAY_LOG.txt` record its run and bytewise replay.

`altered_five_qubit.sqecc` has a deliberately changed magic header.
`examples/ecc_refused.nm` points to it. The executed admission refused that
scenario with status 3 and wrote `refused.receipt`; the rejection also
replayed successfully. `MUTATION_TEST_LOG.txt` records the separate automated
gate test. Run from the extracted repository root:

```sh
./bin/neuromantle surreal qec-verify examples/qecc/five_qubit.sqecc
./bin/neuromantle verify examples/ecc.nm evidence/qecc/admitted.receipt
./bin/neuromantle verify examples/ecc_refused.nm evidence/qecc/refused.receipt
python3 tests/qecc_integration.py
```

This verifies a mathematical quantum error-correcting code certificate.
Neither the certificate gate nor the classical SECDED checkpoint stores the
SNN state in physical quantum memory. Receipt verification assumes the
declared certificate path is resolved from the repository root and the
bundled verifier binary is unchanged.
