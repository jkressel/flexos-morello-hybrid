# SQLite on Unikraft

To build and run this application please use the `kraft` script:

    pip3 install git+https://github.com/unikraft/kraft.git
    mkdir my-first-unikernel && cd my-first-unikernel
    kraft up -p PLATFORM -m ARCHITECTURE sqlite

For more information about `kraft` type ```kraft -h``` or read the
[documentation](http://docs.unikraft.org).
