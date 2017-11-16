# Cuculiform #
Cuculiform is a simple Cuckoo filter implementation as C++11 single-header library,
independent from the [reference implementation](https://github.com/efficient/cuckoofilter).

A Cuckoo filter is a space-efficient probabilistic data structure for fast set membership testing.
They have properties similar to [Bloom filters](https://en.wikipedia.org/wiki/Bloom_filter),
but in contrast, Cuckoo filters support deletion.

For details about the algorithm, see:
["Cuckoo Filter: Practically Better Than Bloom"](http://www.cs.cmu.edu/%7Ebinfan/papers/conext14_cuckoofilter.pdf)
in proceedings of ACM CoNEXT 2014 by Bin Fan, Dave Andersen and Michael Kaminsky

## Build Instructions ##
Cuculiform depends on [HighwayHash](https://github.com/google/highwayhash) being present for its default hash function,
but the hash function is interchangeable.

```bash
git clone https://git.scc.kit.edu/dsn-projects/nsm/cuculiform.git
git clone https://github.com/google/highwayhash.git
cd highwayhash
# Building on Ubuntu has this issue, as of 2017-11-16:
# https://github.com/google/highwayhash/issues/60
# See workaround there.
make -j$(nproc)
cd ../cuculiform
mkdir build
cd build
cmake ..
make
./cuculiform_test
```

## Name Origin ##
cuculiform, def.: cuckoo-like, part of the order [Cuculiformes](https://en.wikipedia.org/wiki/Cuckoo)
