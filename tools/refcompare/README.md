# Reference comparison

Runs the Modplay65 replay engine and the
[micromod](https://github.com/martincameron/micromod) ProTracker replay
(BSD licence, not included here) over the same MOD files and compares
position, period, volume and note triggers of every channel on every tick.

```sh
git clone --depth 1 https://github.com/martincameron/micromod /tmp/micromod
cmake -S tests -B build-tests && cmake --build build-tests
cc -O1 -w -I/tmp/micromod/micromod-c -o build-tests/refdump tools/refcompare/refdump.c
python3 tools/refcompare/compare.py path/to/*.mod
```

Limits: the comparison does not notice a note trigger that the reference
has and Modplay65 lacks when the offset is unchanged, and it does not
cover sample data, loops or the hardware output layer.
