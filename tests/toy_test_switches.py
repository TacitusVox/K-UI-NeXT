# SPDX-License-Identifier: GPL-3.0-only
"""Resolve the Toy Commander test-module switches off in a C source text.

The pinned SCI history tests compare today's shared reader sources with
complete historical files. The Toy test modules (Tests 26 to 82) added
conditionals to those shared sources under their own switches; production
builds never set them. This module removes exactly those conditionals, as a
production build sees them, and keeps every other line as written:

- A switch is off: an #if/#elif on it alone is false, defined() of it is
  false, and #ifdef/#ifndef of it are resolved.
- A condition that mixes a switch with other macros is simplified only as far
  as the switch decides it (`A && !SWITCH` becomes `A`); the rest keeps its
  original text.
- A #define whose value names a switch is dropped and its value remembered, so
  a later #if on that derived name is resolved the same way.

Nothing else is rewritten. The callers bind the result to complete historical
hashes, so an unexpected leftover fails their checks instead of passing.
The production CD music pilot's own switches (KUI_RETAIL_TOY_PILOT,
KUI_TOY_PILOT_*, KUI_TOY_PIPELINE_*) predate these tests and are not touched.
"""
import re

TOY_TEST_SWITCH = re.compile(
    r'KUI_(?:TOY_(?:SUITE|WIRE|SURVEY|CDDA)_\w+|NATIVE_TOY_\w+|DISC_TOY_TRACE\w*|TOY_FMV_COMPARE\w*|'
    r'TOY_BURST_PRODUCER_TEST|TOY_AHEAD_HOT|TOY_LOW_TERMINAL|TOY_SCI_BURST_HOST_TEST|'
    r'TOY_WITNESS_HOST_TEST|TOY_DEMAND_TRACE_H|TOY_MOVIE_EXIT_H|TOY_MOVIE_TRACE_H|'
    r'TOY_RAM_WINDOW_H|TOY_SD_CRC16_H|TOY_TEST_BUILD|TOY_TEST_BUILD_H|'
    r'CD_EDC_CACHED_PAIR|WIRE_STREAM_TEST|WIRE_TEST)\Z')
# The umbrella header only defines KUI_TOY_TEST_BUILD from the switches above.
UMBRELLA_INCLUDE = re.compile(r'\s*#\s*include\s*"kui/toy_test_build\.h"\s*\Z')
TOKEN = re.compile(r'\s*(?:(?P<num>0[xX][0-9A-Fa-f]+|\d+)[uUlL]*|(?P<id>[A-Za-z_]\w*)|'
                   r'(?P<op>&&|\|\||==|!=|<=|>=|<<|>>|[-+*/%<>!~&|^()?:]))')


def is_toy_switch(name):
    return bool(TOY_TEST_SWITCH.match(name))


class Unknown:
    """A part of the condition that the switches do not decide: its text span."""
    __slots__ = ('begin', 'end')

    def __init__(self, begin, end):
        self.begin, self.end = begin, end


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', ' ', text)
    return text.split('//', 1)[0].rstrip()


class Condition:
    """Partial evaluation of one preprocessor condition."""
    BINARY = (('||',), ('&&',), ('|',), ('^',), ('&',), ('==', '!='), ('<', '>', '<=', '>='),
              ('<<', '>>'), ('+', '-'), ('*', '/', '%'))

    def __init__(self, text, derived):
        self.text, self.derived = text, derived
        self.tokens, at = [], 0
        while at < len(text):
            if text[at:].strip() == '':
                break
            match = TOKEN.match(text, at)
            if not match:
                raise AssertionError('Unparsed preprocessor condition: ' + text)
            kind = match.lastgroup
            begin = match.start(kind)
            self.tokens.append((kind, match.group(kind), begin, match.end(kind)))
            at = match.end()
        self.at = 0

    def peek(self):
        return self.tokens[self.at] if self.at < len(self.tokens) else (None, None, len(self.text), len(self.text))

    def take(self, value=None):
        token = self.peek()
        if value is not None and token[1] != value:
            raise AssertionError('Expected %r in preprocessor condition: %s' % (value, self.text))
        self.at += 1
        return token

    def evaluate(self):
        value, begin, end = self.ternary()
        if self.at != len(self.tokens):
            raise AssertionError('Trailing tokens in preprocessor condition: ' + self.text)
        return value, begin, end

    def ternary(self):
        cond, begin, end = self.binary(0)
        if self.peek()[1] != '?':
            return cond, begin, end
        self.take('?')
        yes, _, _ = self.ternary()
        self.take(':')
        no, _, end = self.ternary()
        if isinstance(cond, Unknown):
            return Unknown(begin, end), begin, end
        return (yes if cond else no), begin, end

    def binary(self, level):
        if level == len(self.BINARY):
            return self.unary()
        left, begin, end = self.binary(level + 1)
        while self.peek()[1] in self.BINARY[level]:
            op = self.take()[1]
            right, rbegin, end = self.binary(level + 1)
            left = self.combine(op, left, right, begin, end)
        return left, begin, end

    @staticmethod
    def combine(op, left, right, begin, end):
        lu, ru = isinstance(left, Unknown), isinstance(right, Unknown)
        if op == '&&':
            if (not lu and not left) or (not ru and not right):
                return 0
            if not lu and not ru:
                return 1
            return right if not lu else (left if not ru else Unknown(begin, end))
        if op == '||':
            if (not lu and left) or (not ru and right):
                return 1
            if not lu and not ru:
                return 0
            return right if not lu else (left if not ru else Unknown(begin, end))
        if lu or ru:
            return Unknown(begin, end)
        return {'|': lambda a, b: a | b, '^': lambda a, b: a ^ b, '&': lambda a, b: a & b,
                '==': lambda a, b: int(a == b), '!=': lambda a, b: int(a != b),
                '<': lambda a, b: int(a < b), '>': lambda a, b: int(a > b),
                '<=': lambda a, b: int(a <= b), '>=': lambda a, b: int(a >= b),
                '<<': lambda a, b: a << b, '>>': lambda a, b: a >> b,
                '+': lambda a, b: a + b, '-': lambda a, b: a - b, '*': lambda a, b: a * b,
                '/': lambda a, b: a // b if b else 0, '%': lambda a, b: a % b if b else 0}[op](left, right)

    def unary(self):
        kind, value, begin, end = self.peek()
        if value in ('!', '~', '-', '+'):
            self.take()
            inner, _, end = self.unary()
            if isinstance(inner, Unknown):
                return Unknown(begin, end), begin, end
            return {'!': lambda v: int(not v), '~': lambda v: ~v, '-': lambda v: -v, '+': lambda v: v}[value](inner), begin, end
        return self.primary()

    def primary(self):
        kind, value, begin, end = self.take()
        if value == '(':
            inner, _, _ = self.ternary()
            _, _, _, end = self.take(')')
            return (Unknown(begin, end) if isinstance(inner, Unknown) else inner), begin, end
        if kind == 'num':
            digits = re.match(r'(0[xX][0-9A-Fa-f]+|\d+)', value).group(1)
            base = 16 if digits[:2] in ('0x', '0X') else (8 if len(digits) > 1 and digits[0] == '0' else 10)
            return int(digits, base), begin, end
        if kind == 'id' and value == 'defined':
            parenthesized = self.peek()[1] == '('
            if parenthesized:
                self.take('(')
            _, name, _, end = self.take()
            if parenthesized:
                _, _, _, end = self.take(')')
            if name in self.derived:
                return 1, begin, end
            if is_toy_switch(name):
                return 0, begin, end
            return Unknown(begin, end), begin, end
        if kind == 'id':
            if value in self.derived:
                return self.derived[value], begin, end
            if is_toy_switch(value):
                return 0, begin, end
            return Unknown(begin, end), begin, end
        raise AssertionError('Unparsed preprocessor condition: ' + self.text)


def decide(condition, derived):
    """Return (True|False, None) when the switches decide it, else (None, text)."""
    text = strip_comments(condition).strip()
    parser = Condition(text, derived)
    if not any(kind == 'id' and (is_toy_switch(value) or value in derived)
               for kind, value, _, _ in parser.tokens):
        return None, condition
    value, begin, end = parser.evaluate()
    if isinstance(value, Unknown):
        return None, text[value.begin:value.end]
    return bool(value), None


DIRECTIVE = re.compile(r'\s*#\s*(\w+)\s*(.*?)\s*\Z', re.S)


def resolve_toy_switches(source, origins=None):
    """Resolve every Toy test switch off; see the module's description.

    When origins is a list, it receives the source line index of each output
    line, in order."""
    out, stack, derived = [], [], {}
    if origins is None:
        origins = []
    class Out(list):
        def append(self, text):
            list.append(self, text)
            origins.extend(range(first, first + max(1, text.count('\n'))))
    out = Out()
    # A group is [mode, taken, live_branch]: mode 'resolved' when the switches
    # decide its first branch, 'kept' when it stays in the output.
    def live():
        return all(group[2] for group in stack)

    lines = source.splitlines(keepends=True)
    index = 0
    while index < len(lines):
        first = index
        line = lines[index]
        # Join continued directive lines.
        full = line
        while full.rstrip('\n').endswith('\\') and index + 1 < len(lines) and full.lstrip().startswith('#'):
            index += 1
            full += lines[index]
        index += 1
        match = DIRECTIVE.match(full.rstrip('\n')) if full.lstrip().startswith('#') else None
        word = match.group(1) if match else ''
        rest = (match.group(2) if match else '').replace('\\\n', ' ')
        if word in ('if', 'ifdef', 'ifndef'):
            if not live():
                stack.append(['dead', True, False])
                continue
            if word == 'if':
                value, text = decide(rest, derived)
            else:
                name = strip_comments(rest).strip()
                if is_toy_switch(name) or name in derived:
                    defined = name in derived
                    value, text = (defined if word == 'ifdef' else not defined), None
                else:
                    value, text = None, rest
            if value is None:
                stack.append(['kept', False, True])
                out.append(full if text == rest else '#if ' + text + '\n')
            else:
                stack.append(['resolved', value, value])
            continue
        if word == 'elif':
            group = stack[-1] if stack else None
            if group is None:
                raise AssertionError('#elif without #if')
            if group[0] == 'dead':
                continue
            value, text = decide(rest, derived)
            if group[0] == 'kept':
                if value is False:
                    group[2] = False      # this branch never compiles: drop it
                    continue
                group[2] = True
                if value is True:
                    out.append('#else\n')
                    group[0] = 'kept-final'
                else:
                    out.append(full if text == rest else '#elif ' + text + '\n')
                continue
            if group[0] == 'kept-final':
                group[2] = False
                continue
            # resolved group
            if group[1]:
                group[2] = False
                continue
            if value is None:
                group[0], group[1], group[2] = 'kept', False, True
                name = re.fullmatch(r'defined\s*\(\s*(\w+)\s*\)', text.strip())
                out.append(('#ifdef ' + name.group(1) if name else '#if ' + text) + '\n')
            else:
                group[1] = group[2] = value
            continue
        if word == 'else':
            group = stack[-1] if stack else None
            if group is None:
                raise AssertionError('#else without #if')
            if group[0] == 'dead':
                continue
            if group[0] in ('kept', 'kept-final'):
                if group[0] == 'kept-final':
                    group[2] = False
                    continue
                group[2] = True
                out.append(full)
                continue
            group[2] = not group[1]
            group[1] = True
            continue
        if word == 'endif':
            if not stack:
                raise AssertionError('Unbalanced #endif')
            group = stack.pop()
            if group[0] in ('kept', 'kept-final'):
                out.append(full)
            continue
        if not live():
            continue
        if word == 'include' and UMBRELLA_INCLUDE.match(full.rstrip('\n')):
            continue
        if word == 'define':
            parts = re.match(r'(\w+)(\([^)]*\))?\s*(.*)', rest, re.S)
            name, args, body = parts.group(1), parts.group(2), parts.group(3)
            if is_toy_switch(name) and not args:
                # A default for a switch: remember what a production build sees.
                value, text = decide(body.strip() or '1', derived) if body.strip() else (True, None)
                if value is None:
                    value = Condition(strip_comments(body), derived).evaluate()[0]
                    if isinstance(value, Unknown):
                        raise AssertionError('Switch default not decided: ' + full.strip())
                derived[name] = int(value)
                continue
            mentions = [n for n in re.findall(r'[A-Za-z_]\w*', body) if is_toy_switch(n) or n in derived]
            if mentions and not args:
                value, text = decide(body or '1', derived)
                if value is None:
                    raise AssertionError('Derived switch value not decided: ' + full.strip())
                derived[name] = int(value)
                continue
        if word == 'undef' and (is_toy_switch(rest.strip()) or rest.strip() in derived):
            derived.pop(rest.strip(), None)
            continue
        out.append(full)
    if stack:
        raise AssertionError('Unbalanced conditional')
    return ''.join(out)


def code_tokens(text):
    """The tokens a compiler sees: comments and layout dropped, and the
    equivalent spellings #ifdef X, #if defined(X) and #if defined X made one."""
    text = re.sub(r'/\*.*?\*/|//[^\n]*', ' ', text, flags=re.S)
    text = re.sub(r'#\s*ifdef\s+(\w+)', r'#if defined(\1)', text)
    text = re.sub(r'#\s*ifndef\s+(\w+)', r'#if !defined(\1)', text)
    text = re.sub(r'defined\s*\(\s*(\w+)\s*\)|defined\s+(\w+)',
                  lambda m: 'defined(' + (m.group(1) or m.group(2)) + ')', text)
    return re.findall(r'\w+|[^\w\s]', text)


def same_code(a, b):
    return code_tokens(a) == code_tokens(b)
