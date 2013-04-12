/**
 * We can't include comments directly in a .tesla file because it's a
 * text-based serialisation format for protocol buffers. Instead, let's abuse
 * the C preprocessor a little bit:
 *
 * RUN: clang %cflags -D TESLA -E %s -o %t.tesla
 * RUN: tesla graph %t.tesla -o %t.dot
 * RUN: FileCheck -input-file=%t.dot %s
 */

#ifdef TESLA
automaton {
  // CHECK: digraph automaton_{{[0-9]+}}
  identifier {
    location {
      filename: "example.c"
      line: 314
      counter: 42
    }
  }
  context: ThreadLocal
  expression {
    type: SEQUENCE
    sequence {
      // CHECK: 0 [ label = "state 0\n([[ANY:&#[0-9]+;]],[[ANY:&#[0-9]+;]])" ];
      // CHECK: 0 -> 1 [ label = "example_syscall([[ANY]],[[ANY]],[[ANY]])
      expression {
        type: FUNCTION
        function {
          function {
            name: "example_syscall"
          }
          direction: Entry
          context: Callee
          argument {
            type: Any
            name: "cred"
          }
          argument {
            type: Any
            name: "index"
          }
          argument {
            type: Any
            name: "op"
          }
        }
      }
      // CHECK: 1 [ label = "state 1\n([[ANY]],[[ANY]])" ];
      // CHECK: 1 -> 2 [ label = "security_check([[ANY]],o,[[ANY]])
      expression {
        type: FUNCTION
        function {
          function {
            name: "security_check"
          }
          direction: Exit
          context: Callee
          argument {
            type: Any
          }
          argument {
            type: Variable
            index: 0
            name: "o"
          }
          argument {
            type: Any
          }
          expectedReturnValue {
            type: Constant
            name: "since"
            value: 0
          }
        }
      }
      // After that transition, we know about one referenced variable:
      // CHECK: 2 [ label = "state 2\n(o,[[ANY]])" ];
      // CHECK: 2 -> 3 [ label = "security_check([[ANY]],o,op)
      expression {
        type: FUNCTION
        function {
          function {
            name: "security_check"
          }
          direction: Exit
          context: Callee
          argument {
            type: Any
          }
          argument {
            type: Variable
            index: 0
            name: "o"
          }
          argument {
            type: Variable
            index: 1
            name: "op"
          }
          expectedReturnValue {
            type: Constant
            name: "since"
            value: 0
          }
        }
      }
      // CHECK: 3 [ label = "state 3\n(o,op)" ];
      // CHECK: 3 -> 4 [ label = "NOW"
      expression {
        type: NOW
        now {
          location {
            filename: "example.c"
            line: 46
            counter: 0
          }
        }
      }
    }
  }
  argument {
    type: Variable
    index: 0
    name: "o"
  }
  argument {
    type: Variable
    index: 1
    name: "op"
  }
  // CHECK: label = "example.c:314#42"
}
#endif

