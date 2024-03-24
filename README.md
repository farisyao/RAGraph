<h1 align="center">
    <br>
    RAGraph
</h1>
<p align="center">
    A C++ library for geo-distributed graph processing
</p>


## Building **RAGraph**

### Dependencies


- [CMake](https://cmake.org/) (>=2.8)
- A modern C++ compiler compliant with C++-11 standard. (g++ >= 4.8.1 or clang++ >= 3.3)
- [MPICH](https://www.mpich.org/) (>= 2.1.4) or [OpenMPI](https://www.open-mpi.org/) (>= 3.0.0)
- [glog](https://github.com/google/glog) (>= 0.3.4)
- [HElib](https://github.com/homenc/HElib)


Here are the dependencies for optional features:
- [jemalloc](http://jemalloc.net/) (>= 5.0.0) for better memory allocation;
- [Doxygen](https://www.doxygen.nl/index.html) (>= 1.8) for generating documentation;
- Linux [HUGE_PAGES](http://www.kernel.org/doc/Documentation/vm/hugetlbpage.txt) support, for better performance.

Extra dependencies are required by examples:
- [gflags](https://github.com/gflags/gflags) (>= 2.2.0);
- [Apache Kafka](https://github.com/apache/kafka) (>= 2.3.0);
- [librdkafka](https://github.com/edenhill/librdkafka)(>= 0.11.3);


### Building RAGraph and examples

Once the required dependencies have been installed, go to the root directory of RAGraph and do a out-of-source build using CMake.

```bash
mkdir build && cd build
cmake ..
make -j
```

The building targets include a shared/static library, and two sets of examples: [analytical_apps](./examples/analytical_apps).

Alternatively, you can build a particular target with command:

```bash
make ingress #
```

### Dataset
All datasets we used:

| Graph   | Vertex | Edge | 
| ------- | ------ | ---- |
| Web-Google | 916,428 | 6,078,250 |
|Enwiki-2013 | 4,203,323| 101,311,614|
|Arabic-2005 | 22,744,080| 639,999,458|
|UK-2005 | 39,459,925| 936,364,282|
|Twitter-2010| 41,652,230| 1,468,364,884|


<!---###
RAGraph uses edgelist format dataset, each row consists of [src, des], and the weight graph format is [src, des, weight]. We provide the [Google graph](https://www.cise.ufl.edu/research/sparse/matrices/SNAP/web-Google.html) in the Dataset folder (see [./Dataset/google.e](./Dataset/) and [./Dataset/wgoogle.e](./Dataset/)), and other experimental datasets (e.g., [Enwiki-2013](https://law.di.unimi.it/webdata/enwiki-2013/), [Arabic-2005](https://law.di.unimi.it/webdata/arabic-2005/), [UK-2005](https://law.di.unimi.it/webdata/uk-2005/), and [Twitter-2010](https://snap.stanford.edu/data/twitter-2010.html))  can be converted to the above format to run in RAGraph.
-->
<!---### Examples

As examples, we provide four applications to build RAGraph, they are PageRank (command flag: pagerank), Penalized Hitting Probability (php), Single Source Shortest Path (sssp), and Connected Components (cc). 

To run a specific analytical application, users may use command like this:
```bash
# run PageRank with 4 data centers.
 mpirun -np 4 -hostfile ./hostfile  ../build/ingress -application pagerank -efile ./Dataset/google.e -directed=1 -cilk=true -termcheck_threshold 0.001 -app_concurrency 8 -portion=1  -serialization_prefix ./ser -out_prefix ./result/sum_pagerank

# see more flags info.
./run_app --help
-->
