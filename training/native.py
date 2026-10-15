"""ctypes ABI for the native, deterministic bounded trainer and C policies."""
from __future__ import annotations
import ctypes as C
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CAP=64
ARRAY=C.c_int32*CAP
EVALUATOR=C.CFUNCTYPE(C.c_int64,C.POINTER(C.c_int32),C.c_size_t,C.c_void_p)

class Spec(C.Structure):
    _fields_=[("count",C.c_uint32),("passes",C.c_uint32),
              ("evaluations",C.c_uint32),("trace_capacity",C.c_uint32),
              ("initial",ARRAY),("lower",ARRAY),("upper",ARRAY),("step",ARRAY)]

class Result(C.Structure):
    _fields_=[("parameters",ARRAY),("best_score",C.c_int64),
              ("evaluations",C.c_uint32),("accepted",C.c_uint32)]

class Trace(C.Structure):
    _fields_=[("evaluation",C.c_uint32),("coordinate",C.c_uint32),
              ("pass_index",C.c_uint32),("value",C.c_int32),
              ("score",C.c_int64),("incumbent",C.c_int64),("accepted",C.c_uint8)]

def optimize(start: list[int], low: list[int], high: list[int], step: list[int],
             score, passes: int, budget: int) -> tuple[list[int],int,list[dict]]:
    if not (1<=len(start)<=CAP and len(start)==len(low)==len(high)==len(step)):
        raise ValueError("invalid native training vector")
    spec=Spec()
    spec.count=len(start);spec.passes=passes;spec.evaluations=budget;spec.trace_capacity=budget
    for name,values in (("initial",start),("lower",low),("upper",high),("step",step)):
        for i,v in enumerate(values): getattr(spec,name)[i]=v
    errors=[]
    def callback(ptr,n,_):
        try:
            return int(score([ptr[i] for i in range(n)]))
        except Exception as error:
            errors.append(error)
            return -(1<<63)
    func=EVALUATOR(callback)
    lib=C.CDLL(str(ROOT/"bin/nm-train.so"))
    lib.nm_train_optimize.argtypes=[C.POINTER(Spec),EVALUATOR,C.c_void_p,
                                    C.POINTER(Result),C.POINTER(Trace)]
    lib.nm_train_optimize.restype=C.c_int
    result=Result();records=(Trace*budget)()
    status=lib.nm_train_optimize(C.byref(spec),func,None,C.byref(result),records)
    if errors: raise RuntimeError("native objective refused") from errors[0]
    if status: raise RuntimeError("native training refused")
    trace=[{"evaluation":t.evaluation,"coordinate":None if t.coordinate==2**32-1 else t.coordinate,
            "pass":t.pass_index,"value":t.value,"score":t.score,
            "incumbent":t.incumbent,"accepted":bool(t.accepted)}
           for t in records[:result.evaluations]]
    return list(result.parameters[:len(start)]),result.best_score,trace
