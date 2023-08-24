# BASIK

A small stack-based programming language I made.

---
# Building

## Linux
Run `./build.bash` and then `./out/basik <program.bsk>`.

## Windows
If it will ever work, it will be with msys/git bash, and it should be pretty much the smae command `./build.bash`, although some libs might not work/be on Windows.

## Others
For now no other target is really supported, but if you feel like it could be, try running the build script with `-target [linux-gnu/msys]` to see if it works.

## Notes
You can provide optimization flags (`-OZ`/`-O1`/`-O2`/`-O3`) and debug flags (`-g`) to the build script, and they will be passed to gcc.

---
# Compiling Basik Code
Basik runs on a VM that interprets bytecode, for now I'm too lazy to write a proper parser/lexer and stuff so I just stole python. With `python3 compiler.py <file.py> <output.bsk>`, you should be able to compile very basic python scripts to Basik bytecode, which can be ran by the interpreter.

If everything goes well, you should be able to run 
```sh
./tasks/build.bash && \
python3 ./compiler.py hello_world.py hello_world.bsk && \
./out/basik hello_world.bsk
```
which builds the project, compiles the example program and then runs it.
At the end, you should see `Hello, world !` in the output.