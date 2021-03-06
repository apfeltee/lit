NP: i haven't really have put any effort in updating this readme, but this was forked from [egordorichev/lit](github.com/egordorichev/lit)

a quick overview what I've changed:

 - relaxed parser - brackets no longer need to be on the same line
 - optional switch/case in the virtual machine (could be unstable!)
 - mild attempt to support windows
 - generous code cleanup

# lit

Lit is a dynamicaly-typed language, inspired by highly-hackable Lua and familiar C-styled JavaScipt.
You can try it out [in your browser](https://egordorichev.github.io/lit-html/?github=egordorichev/lit/tests/examples/hello_world.lit)!

|                         | Lua      | JavaScript | Lit      |                                                                                                     |
|-                        |-         |-           |-         |-                                                                                                    |
| Easy classes            | &#x274C; | &#10004;   | &#10004; | [Example](https://egordorichev.github.io/lit-html/?github=egordorichev/lit/tests/examples/oop.lit)                   |
| Arrays start from       | 1 (WHAT) | 0          | 0        | Balance in the universe is restored                                                                 |
| String interpolation    | &#x274C; | &#10004;   | &#10004; | [Example](https://egordorichev.github.io/lit-html/?github=egordorichev/lit/tests/examples/interpolation.lit)         |
| Operator overloading    | &#10004; | &#x274C;   | &#10004; | [Example](https://egordorichev.github.io/lit-html/?github=egordorichev/lit/tests/examples/operator_overloading.lit)  |
| Easy to embed           | &#10004; | &#x274C;   | &#10004; | [Example](https://github.com/egordorichev/lit/blob/master/html/glue/glue.c)                         |
| Has no `undefined`      | &#10004; | &#x274C;   | &#10004; | Seriously, why, JS??                                                                                |
| Syntax sugar like `+=`  | &#x274C; | &#10004;   | &#10004; | [Example](https://egordorichev.github.io/lit-html/?github=egordorichev/lit/tests/examples/syntax_sugar.lit)          |
| Coroutines (fibers)     | &#10004; | &#x274C;   | &#10004; | [Example](https://egordorichev.github.io/lit-html/?github=egordorichev/lit/tests/examples/fibers.lit)                |

Sounds good? [Check out some challenge solutions](https://github.com/egordorichev/lit/tree/master/tests/challenges) or try lit for yourself!

# Building

Lit has no external dependencies (besides Emscripten, but that's only for the HTML5 builds), so you will only need gcc, make & cmake.

_On linux, you will also need to install libreadline:_

```bash
sudo apt-get install libreadline6-dev
```


```bash
git clone https://github.com/egordorichev/lit/ && cd lit 
cmake .
make
sudo make install
```

That should install lit, and you should be able to access it from the console. Let's write our first program:

```js
print("Hello, world!")
```

Just run it, and you should see the familiar message in your terminal:

```bash
lit hello.lit
```

That's it! Now you are ready to face a more serious challenge, like the [examples](https://github.com/egordorichev/lit/tree/master/tests/examples) or [challenges](https://github.com/egordorichev/lit/tree/master/tests/challenges)!
Or alternatively, [lit runs just fine in browsers](https://egordorichev.github.io/lit-html/?github=egordorichev/lit/tests/examples/hello_world.lit).

### Syntax Highlighting

If you want syntax highlighting in Visual Studio Code, see [lit-vscode](https://github.com/egordorichev/lit-vscode).


###### History

This is not the first version of lit. And not second. Sadly, most of the versions got lost along the way, but [you still can checkout previous version of lit](https://github.com/egordorichev/static-lit), that was staticly-typed!
