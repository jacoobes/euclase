# solid waffle (name to change)
- [Virtual Machine](#virtual-machine-specification)
- [Language Specification](#language-specification)
- [Assembler](#assembler)

Build system = [xmake](https://xmake.io/#/)
  
To build:
```
xmake build
```
To run:
```
xmake run main
```
Assembler:
```
xmake run asm
```
Tests:
```
xmake run -g test
```

## Virtual Machine Specification

The virtual machine is a bytecode based interpreter which is inspired from [Crafting Interpreters](https://craftinginterpreters.com/),
which was also inspired by the excellent [Lua interpreter](https://www.lua.org/doc/jucs05.pdf)

### design choices
- **Stack based**, for simplicity of design over register-based virtual machines
- simple C codebase, no language bloat from C++ or other family
- Avoid unnecessary abstraction
- The only dependency was copy and pasted from [ccan code archive](https://github.com/rustyrussell/ccan/blob/master/ccan/darray/darray.h).
    - If I add coroutines, this may not be the only dependency. that stuff too hard for me

**Note (02 / 23 / 25)** The opcodes and format of bytecode are still subject to change.

```
I_HALT         // End execution 
I_LOAD         // Load value at index to stack.
I_NEGATE       // negate a number
I_ADD          // add two values
I_SUB          // subtract two numbers
I_MUL          // multiply two numbers
I_DIV          // divide two numbers
I_MOD          // modulus two numbers
I_LTE          // LTE two numbers
I_GT           //  GTE two numbers
I_CALL         // call function
I_JMP          // jmp to instr ptr
I_JMP_FALSY    // jmp if falsy
I_GET_LOCAL    // Get local variable
I_SET_LOCAL    // SET LOCAL VARIABLE
I_SET_GLOBAL   // set global variable
I_GET_GLOBAL   // get global
I_POP          
I_RETURN      
I_LOOP        
I_CLOSURE     
I_GET_UPVAL   
I_SET_UPVAL   
```

The format below for opcodes is \<NAME\> \<byte\> \<OPS\>

- **\<NAME\>** is the english name for the opcode. 
- **\<byte\>** is  a number from [0-255] which is the numerical representation.
- **\<OPS\>**  is a number of operands this byte takes, whose behavior is explained below.

### Opcodes
  
- **I_HALT 0 0**  
   - Stops execution of the virtual machine entirely and dumps the stack.  
- **I_LOAD 1 1**  
   - Loads a value from a given index into the stack.  
- **I_NEGATE 2 0**  
   - Negates the top value on the stack. Pops one value from stack and pushes a negated version
- **I_ADD 3 0**  
   - Pops two values from the stack, adds them, and pushes the result.  
- **I_SUB 4 0**  
   - Pops two values from the stack, subtracts the second from the first, and pushes the result.  
- **I_MUL 5 0**  
   - Pops two values from the stack, multiplies them, and pushes the result.  
- **I_DIV 6 0**  
   - Pops two values from the stack, divides the first by the second, and pushes the result.  
- **I_MOD 7 0**  
   - Pops two values from the stack, computes the modulus, and pushes the result.  
- **I_LTE 8 0**  
   - Pops two values from the stack, checks if the first is less than or equal to the second, and pushes a boolean result.  
- **I_GT 9 0**  
   - Pops two values from the stack, checks if the first is greater than the second, and pushes a boolean result.  
- **I_CALL 10 1**  
   - Calls a function with a given number of arguments.  
- **I_JMP 11 1**  
   - Jumps to a given instruction pointer.
- **I_JMP_FALSY 12 1**  
   - Pops a value; if falsy, jumps to the given instruction pointer.  
- **I_GET_LOCAL 13 1**  
   - Retrieves a local variable from the current stack frame by index.  
- **I_SET_LOCAL 14 1**  
   - Sets a local variable in the current stack frame by index.  
- **I_SET_GLOBAL 15 1**  
   - Sets a global variable by name. The operand is an index to a string in the constant pool. 
- **I_GET_GLOBAL 16 1**  
   - Retrieves a global variable by name. The operand is an index to a string in the constant pool.
- **I_POP 17 0**  
   - Pops the top value from the stack and discards it.  
- **I_RETURN 18 0**  
   - Returns from the current function, popping the stack frame. If no more callframes exist, the program exits.
- **I_LOOP 19 1**  
   - Loops back to a given instruction pointer (used for loops).  
- **I_CLOSURE 20 1**  
   - Creates a closure with captured upvalues.  
- **I_GET_UPVAL 21 1**  
   - Gets an upvalue from a closure.  
- **I_SET_UPVAL 22 1**  
   - Sets an upvalue in a closure. 


## Language Specification 



### comments

```clj
; this is a comment. It is one line long, and a new line delimits it
```

### numbers

numbers are represented as doubles
```js
01293102931
```

```js
12312321
```
```js
12312321.12312
```

```js
-12312321.12312
```

### strings
strings are interned and are any characters surrounded by quotes `" "`. In addition there are escape characters:
'n', 't', 'r', '"'.
```js
"This is a string. It is very \"stringy\" \n."
```
```js
"12.2312"
```

### Operators

Add:
```clj
(+ 1 2)
```
Sub:
```clj
(- 1 2)
```
Div:
```clj
(/ 10 2)
```
Multiply:
```clj
(* 2 10)
```

### Global variables:
```clj
(var a 10)
```
```clj
(var a (- 200 10))
```
```clj
; undefined variable 'a'
(var a a)
```
### Local scopes
The last expression is returned into the entire expression
```clj
(let (a 10)
  (+ a a)) ; => 20
```
```clj
(let (b 5
      a (+ b 10)) a) ; => 15
```

### Reassigning local variables

**Be careful**: Don't add a space between the `@` and symbol. **Global variables are immutable**
```clj
(let (a 5)
  (@a (- a 5)))
```

### Conditionals

Returns the true result
```clj
(if true 20 15)
```

### Loops

Only `while` loops are implemented. The result of the last iteration is returned
```clj
(let (a 10)
  (while a
    (@a (- a 1))))
```

### Functions

```clj
(fn (add (a b))
  (+ a b))
```

```clj
(fn (fact (x))
    (if (<= x 2) x  (* x (fact (- x 1)))))
```

### Function calls

```clj
(add 10 20)
```

```clj
(fact 10)
```

### Assembler
The virtual machine comes with an assembler which provides low level access to the virtual machine.
the source code is [here](./asm.c). 


as of **2/28/2025** the opcodes it can interpret are not up to parity with the language itself.

```
:main 0                                   
   LOAD 1                                 
   JMP_FALSY 3                            
   LOAD 4                                 
   RETURN                                 
   LOAD 3                                 
   RETURN                                
```

An example of an if else which `returns 4 if true else 3`


#### Labels
Labels are the moral equivalent of a function. In fact, a function will be created in the assembler.

```
:(symbol) <arity>
```
**symbol** is the name of the function. **arity** is the number of arguments for a function.


Current instructions that the assembler can handle:

```
"HALT"
"LOAD"
"ADD"
"SUB"
"MUL"
"DIV"
"MOD"
"NEG"
"POP"
"RETURN"
"CALL"
"GET_LOCAL"
"SET_LOCAL"
"JMP"
"JMP_FALSY"
"LOOP"
```

### Other samples:
Other samples can be found in the [samples directory](./sample)


## todo
- closures
- gc
- benchmarks
- OR bytecode emission
- arrays

## bugs?
- overflow on large numbers (fact 255) would seg fault
    - check for overflow
- garbage collection bugs
- let scopes
- while loops dont accept more than one form

