# Script explanations

The readout-affinity balancer can change the CPU affinities of already running processes and their threads.
One criteria is, that the thread's pthread handle need to be named. `psutil` is a dependency.

The json configuration file holds the application names. Applications with the same name can be further
specified with command line arguments they were launched with. Parent and thread name based CPU masks
are specified.

One can use the script as:

    python readout-affinity.py --pinfile example-cpupin.json

