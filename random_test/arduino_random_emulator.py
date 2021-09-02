import numpy as np

_next = np.uint64(1);

def do_random():
    global _next
    x = _next
    if x==0:
        x = np.int64(123459876)
    hi, lo = np.divmod(x, np.int64(127773), dtype=np.int64)
    x = np.int64(16807) * lo - np.int64(2836) * hi;
    if x < 0:
        x += np.int64(0x7fffffff);
    _next = x
    return x
    
def random(minval, maxval=None):
    if maxval is None:
        maxval = minval
        minval = 0
    return minval + np.remainder(do_random(), maxval-minval, dtype=np.int64)

def randomSeed(seed):
    global _next
    if seed>0:
        _next = seed
