#!/usr/bin/env python3
"""Proof figures for verif/rocq/ADM.v (docs/PROOF.typ).

The cases that ADM.v closes with its automation (crush, act) are redone here
by the same procedure -- case analysis on decidable equalities, instances of
the invariant clauses, forward chaining, equational reasoning -- and every
step is recorded as an inference of natural deduction.

  prooftree.py typ > docs/proof-gen.typ
      the figures
  prooftree.py rocq rocq/ADM.v rocq/ADMRegions.v DIR
      DIR/FigADM.v and DIR/FigRegions.v: one lemma for each figure, proved
      with one tactic step for each inference of the figure
"""
import re
import sys
from itertools import combinations, product


# --- terms and formulas -------------------------------------------------------

def C(n): return ('c', n)
def V(n): return ('v', n)
def F(f, *a): return ('f', f) + tuple(a)
def eq(a, b): return ('eq', a, b)
def neq(a, b): return ('not', ('eq', a, b))
def And(*a): return ('and',) + a
def Or(*a): return ('or',) + a
def Imp(a, b): return ('imp', a, b)
def Not(a): return ('not', a)
def All(v, sort, body): return ('all', v, sort, body)
def Ex(v, sort, body): return ('ex', v, sort, body)


BOT = ('bot',)
TOP = C('top')


def subst_t(t, v, w):
    if t == v:
        return w
    if t[0] == 'f':
        return t[:2] + tuple(subst_t(a, v, w) for a in t[2:])
    return t


def subst(f, v, w):
    k = f[0]
    if k == 'eq':
        return ('eq', subst_t(f[1], v, w), subst_t(f[2], v, w))
    if k in ('all', 'ex'):
        return (k, f[1], f[2], subst(f[3], v, w))
    return (k,) + tuple(subst(g, v, w) for g in f[1:])


def subterms(t, acc):
    if t in acc:
        return
    acc.add(t)
    if t[0] == 'f':
        for a in t[2:]:
            subterms(a, acc)


def terms_of(f, acc):
    k = f[0]
    if k == 'eq':
        subterms(f[1], acc)
        subterms(f[2], acc)
    elif k in ('all', 'ex'):
        terms_of(f[3], acc)
    else:
        for g in f[1:]:
            terms_of(g, acc)
    return acc


# --- congruence closure with injective, pairwise distinct constructors --------

INJ = {'Enc', 'Sm', 'Some'}


class CC:
    def __init__(self, eqs, terms=()):
        ts = set()
        for a, b in eqs:
            subterms(a, ts)
            subterms(b, ts)
        for t in terms:
            subterms(t, ts)
        self.p = {t: t for t in ts}
        for a, b in eqs:
            self.union(a, b)
        apps = [t for t in ts if t[0] == 'f']
        changed = True
        while changed:
            changed = False
            sig, cls = {}, {}
            for t in apps:
                k = (t[1],) + tuple(self.find(a) for a in t[2:])
                if k in sig:
                    changed |= self.union(sig[k], t)
                else:
                    sig[k] = t
            for t in apps:
                if t[1] in INJ:
                    k = (t[1], self.find(t))
                    if k in cls:
                        for a, b in zip(t[2:], cls[k][2:]):
                            changed |= self.union(a, b)
                    else:
                        cls[k] = t
        self.clash = None
        heads = {}
        for t in sorted(ts):
            hd = t if t[0] == 'c' else t[1] if t[0] == 'f' and t[1] in INJ else None
            if hd is None:
                continue
            r = self.find(t)
            if r in heads and heads[r][0] != hd:
                self.clash = (heads[r][1], t)
                break
            heads.setdefault(r, (hd, t))

    def find(self, t):
        while self.p[t] != t:
            self.p[t] = self.p[self.p[t]]
            t = self.p[t]
        return t

    def union(self, a, b):
        a, b = self.find(a), self.find(b)
        if a == b:
            return False
        self.p[a] = b
        return True

    def same(self, a, b):
        return self.find(a) == self.find(b)


# --- derivations ----------------------------------------------------------------

_id = [0]


def newid():
    _id[0] += 1
    return _id[0]


class N:
    def __init__(self, rule, concl, prem=(), dis=(), hyp=None, info=None):
        self.rule, self.concl, self.prem, self.dis, self.hyp = rule, concl, list(prem), list(dis), hyp
        self.info = info
        if hyp is not None:
            self.free = frozenset([hyp])
        else:
            self.free = frozenset().union(*[p.free for p in self.prem]) - set(dis)
        self.size = 1 + sum(p.size for p in self.prem)


def hyp(f):
    i = newid()
    return i, N(None, f, hyp=i)


class Fact:
    n = 0

    def __init__(self, label, ants, concl, major=None, prio=1, info=None):
        self.info = info
        Fact.n += 1
        self.uid, self.label, self.ants, self.concl, self.major, self.prio = Fact.n, label, ants, concl, major, prio


class Ctx:
    def __init__(self):
        self.eqs, self.neqs, self.ors, self.facts, self.alls = [], [], [], [], []
        self.have, self.fired, self.split, self.terms = {}, set(), set(), set()
        self.sorts = {'E': [], 'H': [], 'A': []}
        self.schemas, self.done = [], set()
        self.bot, self._cc, self.wrap = None, None, []

    def copy(self):
        c = Ctx()
        c.eqs, c.neqs, c.ors, c.facts, c.alls = self.eqs[:], self.neqs[:], self.ors[:], self.facts[:], self.alls[:]
        c.have, c.fired, c.split, c.terms = dict(self.have), set(self.fired), set(self.split), set(self.terms)
        c.sorts = {k: v[:] for k, v in self.sorts.items()}
        c.schemas, c.done = self.schemas, set(self.done)
        c.bot, c._cc = self.bot, self._cc
        return c

    def cc(self):
        if self._cc is None:
            self._cc = CC([(a, b) for a, b, _ in self.eqs], self.terms)
        return self._cc

    def need(self, f):
        ts = terms_of(f, set())
        if not ts <= self.terms:
            self.terms |= ts
            self._cc = None

    def same(self, a, b):
        self.need(eq(a, b))
        return self.cc().same(a, b)

    def eqproof(self, a, b):
        if a == b:
            return N('=', eq(a, b))
        for x, y, p in self.eqs:
            if (x, y) == (a, b):
                return p
        keep = list(self.eqs)
        for l in sorted(self.eqs, key=lambda l: -l[2].size):
            t = [m for m in keep if m is not l]
            if CC([(x, y) for x, y, _ in t], [a, b]).same(a, b):
                keep = t
        return N('=', eq(a, b), [p for _, _, p in keep])

    def eqprem(self, a, b):
        for x, y, p in self.eqs:
            if (x, y) == (b, a):
                return p
        return self.eqproof(a, b)

    def inconsistent(self):
        if self.bot:
            return self.bot
        best = None
        cc = self.cc()
        if cc.clash:
            best = N('dis', BOT, [self.eqproof(*cc.clash)])
        for a, b, pr in self.neqs:
            if cc.same(a, b):
                p = N('¬E', BOT, [pr, self.eqproof(a, b)])
                if best is None or p.size < best.size:
                    best = p
        return best

    def refuted(self, a, b):
        cc = CC([(x, y) for x, y, _ in self.eqs] + [(a, b)], self.terms)
        return cc.clash or any(cc.same(x, y) for x, y, _ in self.neqs)

    def leaf(self, f):
        if f in self.have:
            return self.have[f]
        k = f[0]
        if k == 'eq':
            return self.eqproof(f[1], f[2]) if self.same(f[1], f[2]) else None
        if k != 'bot':
            self.need(f)
            best = None
            for g, pr in self.have.items():
                pairs = []
                if g[0] == k and self.match(f, g, pairs):
                    p = N('=', f, [pr] + [self.eqprem(a, b) for a, b in dict.fromkeys(pairs)])
                    if best is None or p.size < best.size:
                        best = p
            if best:
                return best
        if k == 'not' and f[1][0] == 'eq':
            self.need(f)
            if not self.refuted(f[1][1], f[1][2]):
                return None
            c = self.copy()
            i, h = hyp(f[1])
            c.add(f[1], h)
            return N('¬I', f, [c.inconsistent()], [i])
        if k == 'and':
            ps = [self.leaf(g) for g in f[1:]]
            return None if None in ps else N('∧I', f, ps)
        if k == 'or':
            for g in f[1:]:
                p = self.leaf(g)
                if p:
                    return N('∨I', f, [p])
            return None
        if k == 'bot':
            return self.inconsistent()
        return None

    def match(self, f, g, pairs):
        if f[0] != g[0] or len(f) != len(g):
            return False
        if f[0] in ('all', 'ex'):
            return f[1:3] == g[1:3] and self.match(f[3], g[3], pairs)
        if f[0] == 'eq':
            fs, gs = f[1:], g[1:]
            if f[2] == TOP == g[2] and f[1][:2] == g[1][:2] and len(f[1]) == len(g[1]):
                fs, gs = f[1][2:], g[1][2:]
            for a, b in zip(gs, fs):
                if a != b:
                    if not self.cc().same(a, b):
                        return False
                    pairs.append((a, b))
            return True
        return all(self.match(a, b, pairs) for a, b in zip(f[1:], g[1:]))

    def add(self, f, pr, prio=1):
        if f in self.have:
            return
        self.have[f] = pr
        self.need(f)
        k = f[0]
        if k == 'eq':
            self.eqs.append((f[1], f[2], pr))
            self._cc = None
        elif k == 'not' and f[1][0] == 'eq':
            self.neqs.append((f[1][1], f[1][2], pr))
        elif k == 'not':
            self.facts.append(Fact('¬E', [f[1]], BOT, pr, prio))
        elif k == 'and':
            for g in f[1:]:
                self.add(g, N('∧E', g, [pr]), prio)
        elif k == 'or':
            self.ors.append((prio, len(self.ors), f, pr))
        elif k == 'imp':
            self.facts.append(Fact('→E', [f[1]], f[2], pr, prio))
        elif k == 'bot':
            self.bot = pr
        elif k == 'all':
            self.alls.append((f, pr, prio))
            for t in self.sorts[f[2]]:
                g = subst(f[3], f[1], t)
                self.add(g, N('∀E', g, [pr]), prio)
        elif k == 'ex':
            n, taken = 1, {t for ts in self.sorts.values() for t in ts}
            while V(f[1][1].rstrip("'") + str(n)) in taken:
                n += 1
            v = V(f[1][1].rstrip("'") + str(n))
            g = subst(f[3], f[1], v)
            i, h = hyp(g)
            self.wrap.append((pr, i))
            self.newterm(f[2], v)
            self.add(g, h, prio)

    def newterm(self, sort, v):
        if v in self.sorts[sort]:
            return
        if sort in 'EH':
            for t in self.sorts[sort]:
                f = Or(eq(v, t), neq(v, t))
                self.add(f, N('dec', f), 2)
        self.sorts[sort].append(v)
        for f, pr, prio in list(self.alls):
            if f[2] == sort:
                g = subst(f[3], f[1], v)
                self.add(g, N('∀E', g, [pr]), prio)
        self.instantiate()

    def instantiate(self):
        for name, label, sorts, fn in self.schemas:
            for asg in product(*[self.sorts[c] for c in sorts]):
                if (name, asg) not in self.done:
                    self.done.add((name, asg))
                    for j, (ants, concl) in enumerate(fn(*asg)):
                        self.addfact(Fact(label, ants, concl, info=(name, j, asg)))

    def addfact(self, fc):
        if fc.ants:
            self.facts.append(fc)
            for a in fc.ants:
                self.need(a)
            self.need(fc.concl)
        else:
            self.add(fc.concl, N(fc.label, fc.concl, info=fc.info), fc.prio)

    def saturate(self):
        again = True
        while again:
            again = False
            if self.bot or self.cc().clash:
                return
            for fc in list(self.facts):
                if fc.uid in self.fired:
                    continue
                ps = []
                for a in fc.ants:
                    p = self.leaf(a)
                    if p is None:
                        break
                    ps.append(p)
                else:
                    self.fired.add(fc.uid)
                    n = N(fc.label, fc.concl, ([fc.major] if fc.major else []) + ps, info=fc.info)
                    self.add(fc.concl, n, fc.prio)
                    again = True

    def next_or(self):
        for _, _, f, pr in sorted(self.ors, key=lambda o: o[:2]):
            if f in self.split or any(g in self.have for g in f[1:]):
                continue
            return f, pr
        return None

    def finish(self, p):
        for expr, i in reversed(self.wrap):
            if i in p.free:
                p = N('∃E', p.concl, [expr, p], [i])
        return p


def solve(ctx, goal):
    k = goal[0]
    if k in ('imp', 'not'):
        a, b = goal[1], goal[2] if k == 'imp' else BOT
        c = ctx.copy()
        i, h = hyp(a)
        c.add(a, h)
        p = solve(c, b)
        return p and ctx.finish(N('→I' if k == 'imp' else '¬I', goal, [p], [i]))
    if k == 'all':
        c = ctx.copy()
        c.newterm(goal[2], goal[1])
        p = solve(c, goal[3])
        return p and ctx.finish(N('∀I', goal, [p]))
    ctx.saturate()
    b = ctx.inconsistent()
    if b:
        return ctx.finish(b if goal == BOT else N('⊥E', goal, [b]))
    p = ctx.leaf(goal)
    if p:
        return ctx.finish(p)
    o = ctx.next_or()
    if o:
        f, pr = o
        ps, ids = [], []
        for g in f[1:]:
            c = ctx.copy()
            c.split.add(f)
            i, h = hyp(g)
            c.add(g, h)
            p = solve(c, goal)
            if p is None:
                return None
            if i not in p.free:
                return ctx.finish(p)
            ps.append(p)
            ids.append(i)
        return ctx.finish(N('∨E', goal, [pr] + ps, ids))
    if k == 'and':
        ps = [solve(ctx.copy(), g) for g in goal[1:]]
        return None if None in ps else ctx.finish(N('∧I', goal, ps))
    if k == 'ex':
        for w in ctx.sorts[goal[2]]:
            p = solve(ctx.copy(), subst(goal[3], goal[1], w))
            if p:
                return ctx.finish(N('∃I', goal, [p]))
    if k == 'or':
        for g in goal[1:]:
            if g[0] != 'eq':
                p = solve(ctx.copy(), g)
                if p:
                    return ctx.finish(N('∨I', goal, [p]))
    return None


# --- the model of ADM.v ---------------------------------------------------------

FIELDS = ['st', 'sh', 'mt', 'ph', 'rn', 'cx', 'pm', 'mm', 'sec', 'ls', 'dt', 'sm', 'dd', 'sn', 'epm', 'adm']


class St:
    def __init__(self, prime):
        for f in FIELDS:
            setattr(self, f, (lambda g: lambda *a: F(g, *a))(f + ("'" if prime else '')))


S, S_ = St(False), St(True)
for _c in ['INVALID', 'FRESH', 'RUNNING', 'STOPPED', 'DESTROYING', 'IDLE', 'REVOKING', 'VALIDATING',
           'ZEROED', 'sNONE', 'sRONLY', 'sRW', 'pNONE', 'pR', 'pRW', 'botH', 'botM', 'tt', 'ff',
           'Host', 'knone', 'sharenone', 'sharero', 'sharerw', 'invol', 'exit', 'EMPTY']:
    globals()[_c] = C(_c)
E0, H0, X, Y, Z = V('e'), V('h'), V('x'), V('y'), V('z')
D0, M0, SV, MV, ZERO = V('d'), V('m0'), V('s'), V('m'), V('zero')
EB, HB, AV, PV, RE, RA = V("e'"), V("h'"), V('a'), V('p'), V('Re'), V('Ra')


def Enc(e): return F('Enc', e)
def Sm(e): return F('Sm', e)
def Some(t): return F('Some', t)
def P(name, *a): return eq(F(name, *a), TOP)
def Keep(k): return Or(eq(k, sharenone), eq(k, sharero), eq(k, invol), eq(k, exit))
def WF(d, m): return P('WF', d, m)
def In(a, r): return P('in', a, r)
def Prot(s, x, a): return Or(In(a, s.epm(x)), In(a, s.adm(x)))
def Empty(r): return All(AV, 'A', Not(In(AV, r)))


# name, sorts of the variables, antecedents and consequent
CLAUSES = [
    ('S1', 'EH', lambda s, x, y: ([Or(eq(s.cx(y), Enc(x)), eq(s.cx(y), Sm(x)))], eq(s.rn(x), Some(y)))),
    ('S2', 'EH', lambda s, x, y: ([eq(s.rn(x), Some(y))], And(
        eq(s.st(x), RUNNING),
        Or(And(eq(s.ph(x), IDLE), eq(s.cx(y), Enc(x))),
           And(Or(eq(s.ph(x), REVOKING), eq(s.ph(x), VALIDATING)), eq(s.cx(y), Sm(x))))))),
    ('S3', 'E', lambda s, x: ([eq(s.st(x), RUNNING)], neq(s.rn(x), botH))),
    ('S4', 'E', lambda s, x: ([neq(s.st(x), RUNNING)], Or(
        eq(s.ph(x), IDLE), And(eq(s.ph(x), ZEROED), eq(s.st(x), DESTROYING))))),
    ('S5', 'EH', lambda s, x, y: ([eq(s.cx(y), Enc(x))], And(eq(s.sh(x), sNONE), eq(s.mt(x), botM)))),
    ('A2', 'EHH', lambda s, x, y, z: ([eq(s.cx(y), Enc(x))], And(
        eq(s.pm(x, y), pRW), Imp(neq(z, y), eq(s.pm(x, z), pNONE))))),
    ('B2', 'EH', lambda s, x, y: ([eq(s.ph(x), VALIDATING)], eq(s.pm(x, y), pNONE))),
    ('A3', 'EH', lambda s, x, y: ([neq(s.cx(y), Enc(x)), neq(s.st(x), INVALID)], And(
        Imp(neq(s.pm(x, y), pNONE), neq(s.sh(x), sNONE)),
        Imp(eq(s.pm(x, y), pRW), eq(s.sh(x), sRW))))),
    ('D2c', 'EH', lambda s, x, y: ([eq(s.st(x), INVALID)], And(
        eq(s.sh(x), sNONE), eq(s.mt(x), botM), eq(s.pm(x, y), pNONE)))),
    ('A1', 'EH', lambda s, x, y: ([eq(s.st(x), FRESH)], eq(s.pm(x, y), pNONE))),
    ('A8', 'EH', lambda s, x, y: ([eq(s.st(x), STOPPED), Or(eq(s.ls(x), invol), eq(s.ls(x), exit))], And(
        eq(s.sh(x), sNONE), eq(s.pm(x, y), pNONE)))),
    ('E1', 'EH', lambda s, x, y: ([eq(s.sec(x), tt), neq(s.cx(y), Enc(x))], eq(s.pm(x, y), pNONE))),
    ('Z', 'E', lambda s, x: ([eq(s.ph(x), ZEROED)], eq(s.sec(x), ff))),
    ('Dirty', 'E', lambda s, x: ([eq(s.dt(x), tt)], eq(s.sh(x), sRW))),
    ('B5', 'E', lambda s, x: ([eq(s.st(x), FRESH)], And(
        eq(s.sh(x), sRW), eq(s.dt(x), tt), eq(s.ls(x), knone)))),
    ('D2b', 'E', lambda s, x: ([eq(s.sh(x), sRW), neq(s.st(x), INVALID)], eq(s.mt(x), s.sm(x)))),
    ('B3', 'E', lambda s, x: ([eq(s.dd(x), tt)], Or(eq(s.st(x), DESTROYING), eq(s.st(x), INVALID)))),
    ('L', 'E', lambda s, x: ([Or(eq(s.st(x), STOPPED), eq(s.st(x), RUNNING)), Keep(s.ls(x))],
                             neq(s.sh(x), sRW))),
    ('E2', 'E', lambda s, x: ([Or(eq(s.st(x), STOPPED), And(eq(s.st(x), RUNNING), neq(s.ph(x), IDLE))),
                               Keep(s.ls(x))], eq(s.mm(x), s.sn(x)))),
]
CL = {c[0]: c for c in CLAUSES}
# the clauses each preservation lemma of ADM.v hands to crush
PRES_FACTS = {
    'S1': 'S1 S2', 'S2': 'S1 S2', 'S3': 'S3', 'S4': 'S1 S2 S4', 'S5': 'S1 S2 S5',
    'A2': 'S1 S2 A2 B2', 'B2': 'S1 S2 S4 A1 B2', 'A3': 'S1 S2 A2 B2 A3', 'D2c': 'S1 S2 S4 D2c',
    'A1': 'S1 S2 S4 A1', 'A8': 'S1 S2 S4 A2 A8', 'E1': 'S1 S2 A2 B2 E1', 'Z': 'S1 S2 Z',
    'Dirty': 'S1 S2 S5 A3 Dirty', 'B5': 'S1 S2 S4 B5', 'D2b': 'S2 D2b', 'B3': 'S1 S2 B3',
    'L': 'B5 L', 'E2': 'S1 S2 A3 B5 L E2',
}
# name -> label, sorts, function to a list of (antecedents, consequent)
SCHEMAS = {c[0]: (c[0], c[1], (lambda fn: lambda *a: [fn(S, *a)])(c[2])) for c in CLAUSES}
SCHEMAS['running_of_ctx'] = ('running_of_ctx', 'EH', lambda x, y: [(
    [Or(eq(S.cx(y), Enc(x)), eq(S.cx(y), Sm(x)))], And(eq(S.st(x), RUNNING), eq(S.rn(x), Some(y))))])
SCHEMAS['running_of_phase'] = ('running_of_phase', 'E', lambda x: [(
    [Or(eq(S.ph(x), REVOKING), eq(S.ph(x), VALIDATING))], eq(S.st(x), RUNNING))])
SCHEMAS['C1'] = ('C1', 'EEA', lambda x, w, a: [
    ([In(a, S.epm(x)), In(a, S.adm(x))], BOT),
    ([Prot(S, x, a), P('SMR', a)], BOT),
    ([neq(x, w), Prot(S, x, a), Prot(S, w, a)], BOT)])
SCHEMAS['IH'] = ('IH', 'E', lambda x: [
    ([eq(S.st(x), INVALID)], And(Empty(S.epm(x)), Empty(S.adm(x)))),
    ([And(Empty(S.epm(x)), Empty(S.adm(x)))], eq(S.st(x), INVALID))])
SCHEMAS['empty'] = ('def', 'A', lambda a: [([In(a, EMPTY)], BOT)])
SCHEMAS['NoOverlap'] = ('pre', 'EA', lambda w, a: [
    ([In(a, RE), In(a, RA)], BOT),
    ([Or(In(a, RE), In(a, RA)), P('SMR', a)], BOT),
    ([neq(w, E0), Or(In(a, RE), In(a, RA)), Prot(S, w, a)], BOT)])
SCHEMAS['SameInvalid'] = ('pre', 'E', lambda x: [
    ([eq(S.st(x), INVALID)], eq(S_.st(x), INVALID)), ([eq(S_.st(x), INVALID)], eq(S.st(x), INVALID))])


def E(v): return ('E', v)
def Hu(v): return ('H', v)
def PMALL(v): return ('PMALL', v)
def PMONE(v): return ('PMONE', v)
def COND(*cs): return ('cond', cs)


ANY = ('any',)


def by_share(t, none, ronly, rw):
    return COND((eq(t, sNONE), none), (eq(t, sRONLY), ronly), (eq(t, sRW), rw))


class Rule:
    def __init__(self, name, has_h, guards, upd, axioms=(), has_e=True, schemas=()):
        self.name, self.has_h, self.guards, self.upd, self.axioms = name, has_h, guards, upd, axioms
        self.has_e, self.schemas = has_e, schemas


def stop(name, s, m, k, guards, axioms=()):
    return Rule(name, True, [eq(S.cx(H0), Enc(E0)), eq(S.st(E0), RUNNING)] + guards, dict(
        st=E(STOPPED), sh=E(s), mt=E(m), rn=E(botH), cx=Hu(Host),
        pm=by_share(s, PMONE(pNONE), PMALL(pR), PMALL(pRW)),
        sec=by_share(s, None, E(ff), E(ff)), ls=k,
        dt=by_share(s, None, None, E(tt)), sm=by_share(s, None, None, E(m)),
        sn=E(S.mm(E0))), axioms)


def exh(t): return Or(eq(t, sNONE), eq(t, sRONLY), eq(t, sRW))


RULES = [
    Rule('Create', True, [eq(S.cx(H0), Host), eq(S.st(E0), INVALID)], dict(
        st=E(FRESH), sh=E(sRW), mt=E(Some(M0)), pm=PMALL(pNONE), mm=E(D0), sec=E(ff),
        ls=E(knone), dt=E(tt), sm=E(Some(M0)), dd=E(ff))),
    Rule('RunBegin', True, [eq(S.cx(H0), Host), eq(S.st(E0), FRESH)], dict(
        st=E(RUNNING), ph=E(VALIDATING), rn=E(Some(H0)), cx=Hu(Sm(E0)))),
    Rule('ResumeBegin', True, [eq(S.cx(H0), Host), eq(S.st(E0), STOPPED), eq(S.ph(E0), IDLE)], dict(
        st=E(RUNNING), ph=E(REVOKING), rn=E(Some(H0)), cx=Hu(Sm(E0)))),
    Rule('RevokeStep', True, [eq(S.ph(E0), REVOKING), neq(S.pm(E0, H0), pNONE)], dict(pm=PMONE(pNONE))),
    Rule('RevokeDone', False, [eq(S.ph(E0), REVOKING), All(HB, 'H', eq(S.pm(E0, HB), pNONE))],
         dict(ph=E(VALIDATING))),
    Rule('ValidatePass', True,
         [eq(S.ph(E0), VALIDATING), eq(S.rn(E0), Some(H0)),
          Or(neq(S.sh(E0), sRW), WF(S.mm(E0), S.mt(E0)))], dict(
             sh=E(sNONE), mt=E(botM), ph=E(IDLE), cx=Hu(Enc(E0)), pm=PMONE(pRW),
             dt=by_share(S.sh(E0), None, None, E(ff))), [exh(S.sh(E0))]),
    Rule('ValidateFail', True,
         [eq(S.ph(E0), VALIDATING), eq(S.rn(E0), Some(H0)), eq(S.sh(E0), sRW),
          Not(WF(S.mm(E0), S.mt(E0)))], dict(
             st=E(DESTROYING), ph=E(IDLE), rn=E(botH), cx=Hu(Host), dd=E(tt))),
    stop('StopShare', SV, MV, by_share(SV, E(sharenone), E(sharero), E(sharerw)),
         [Imp(eq(SV, sRW), neq(MV, botM)), Imp(neq(MV, botM), eq(SV, sRW))], [exh(SV)]),
    stop('StopInvoluntary', sNONE, botM, E(invol), []),
    stop('Exit', sNONE, botM, E(exit), []),
    Rule('DestroyBegin', True,
         [eq(S.cx(H0), Host), Or(eq(S.st(E0), FRESH), eq(S.st(E0), STOPPED)), eq(S.ph(E0), IDLE)],
         dict(st=E(DESTROYING))),
    Rule('DestroyZero', False, [eq(S.st(E0), DESTROYING), eq(S.ph(E0), IDLE)], dict(
        ph=E(ZEROED), mm=E(ZERO), sec=E(ff))),
    Rule('DestroyFree', False, [eq(S.st(E0), DESTROYING), eq(S.ph(E0), ZEROED)], dict(
        st=E(INVALID), sh=E(sNONE), mt=E(botM), ph=E(IDLE), pm=PMALL(pNONE), ls=E(knone),
        dt=E(ff), sm=E(botM))),
    Rule('EncWrite', True, [eq(S.cx(H0), Enc(E0)), eq(S.pm(E0, H0), pRW)], dict(mm=E(D0), sec=E(tt))),
    Rule('AdvWrite', True,
         [Or(eq(S.cx(H0), Host), Ex(EB, 'E', And(neq(EB, E0), eq(S.cx(H0), Enc(EB))))),
          neq(S.st(E0), INVALID), eq(S.pm(E0, H0), pRW)], dict(mm=E(D0), sec=E(ff), dt=E(tt))),
]
RULE = {r.name: r for r in RULES}
# the transitions of ADMRegions.v
RRULES = [
    Rule('SR_Create', True, RULE['Create'].guards + [Not(Empty(RE)), Not(Empty(RA))],
         dict(RULE['Create'].upd, epm=E(RE), adm=E(RA)), schemas=['NoOverlap']),
    Rule('SR_DestroyFree', False, RULE['DestroyFree'].guards,
         dict(RULE['DestroyFree'].upd, epm=E(EMPTY), adm=E(EMPTY))),
    Rule('SR_Other', False, [], {f: ANY for f in FIELDS[:-2]}, has_e=False, schemas=['SameInvalid']),
]
INIT = dict(st=INVALID, sh=sNONE, mt=botM, ph=IDLE, rn=botH, cx=Host, pm=pNONE, mm=ZERO, sec=ff,
            ls=knone, dt=ff, sm=botM, dd=ff, sn=ZERO, epm=EMPTY, adm=EMPTY)


def post_facts(rule, t):
    f, args = t[1][:-1], t[2:]
    old, new = F(f, *args), t

    def go(spec, conds):
        if spec is None:
            return [Fact('post', conds, eq(new, old))]
        k = spec[0]
        if k == 'any':
            return []
        if k == 'cond':
            out = []
            for c, sp in spec[1]:
                if c[1][0] == 'c' and c[2][0] == 'c':
                    if c[1] == c[2]:
                        out += go(sp, conds)
                else:
                    out += go(sp, conds + [c])
            return out
        v = spec[1]
        if k == 'E':
            out = [Fact('post', conds, eq(F(t[1], E0), v))]
        elif k == 'H':
            out = [Fact('post', conds, eq(F(t[1], H0), v))]
            if args[0] != H0:
                out.append(Fact('post', conds + [neq(args[0], H0)], eq(new, old)))
            return out
        elif k == 'PMALL':
            out = [Fact('post', conds, eq(F(t[1], E0, args[1]), v))]
        elif k == 'PMONE':
            out = [Fact('post', conds, eq(F(t[1], E0, H0), v))]
            if args[1] != H0:
                out.append(Fact('post', conds + [neq(args[1], H0)],
                                eq(F(t[1], E0, args[1]), F(f, E0, args[1]))))
        if args[0] != E0:
            out.append(Fact('post', conds + [neq(args[0], E0)], eq(new, old)))
        return out
    return go(rule.upd.get(f), [])


def ground_terms(f, sorts, acc):
    if f[0] in ('all', 'ex'):
        for t in sorts[f[2]] + [f[1]]:
            ground_terms(subst(f[3], f[1], t), sorts, acc)
    elif f[0] == 'eq':
        terms_of(f, acc)
    else:
        for g in f[1:]:
            ground_terms(g, sorts, acc)
    return acc


def dec(a, b): return Or(eq(a, b), neq(a, b))


def setup(rule, evars, hvars, facts, goal, axioms=(), extra=(), avars=(), init=False):
    """Context of one case: the premises of the rule, the instances of the
    invariant clauses, and the post-state of every primed term of the goal."""
    ctx = Ctx()
    ets = ([E0] if rule and rule.has_e else []) + list(evars)
    hts = ([H0] if rule and rule.has_h else []) + list(hvars)
    ctx.sorts = {'E': ets[:], 'H': hts[:], 'A': list(avars)}
    if rule and rule.has_e:
        for x in evars:
            f = dec(x, E0)
            ctx.add(f, N('dec', f), 0)
    for a, b in [(a, b) for i, a in enumerate(evars) for b in evars[i + 1:]]:
        f = dec(a, b)
        ctx.add(f, N('dec', f), 3)
    if rule:
        for g in rule.guards:
            ctx.add(g, N('pre', g))
    ctx.schemas = [(n,) + SCHEMAS[n] for n in list(facts) + list(rule.schemas if rule else [])]
    ctx.instantiate()
    for fc in extra:
        ctx.addfact(fc)
    if rule or init:
        seen = set()
        for t in sorted(ground_terms(goal, ctx.sorts, set())):
            if t[0] != 'f':
                continue
            if init and t[1] in INIT:
                fcs = [Fact('init', [], eq(t, INIT[t[1]]))]
            elif rule and t[1].endswith("'"):
                fcs = post_facts(rule, t)
            else:
                continue
            for fc in fcs:
                key = (tuple(fc.ants), fc.concl)
                if key not in seen:
                    seen.add(key)
                    ctx.addfact(fc)
    if rule and rule.has_h:
        for y in hvars:
            f = dec(y, H0)
            ctx.add(f, N('dec', f), 2)
    for f in (rule.axioms if rule else ()):
        ctx.add(f, N('exh', f), 2)
    for f, lab in axioms:
        ctx.add(f, N(lab, f), 2)
    for a, b in [(a, b) for i, a in enumerate(hvars) for b in hvars[i + 1:]]:
        f = dec(a, b)
        ctx.add(f, N('dec', f), 3)
    return ctx


def clause_goal(name, s):
    _, sorts, fn = CL[name]
    hs = iter([Y, Z])
    vs = [X if c == 'E' else next(hs) for c in sorts]
    ants, concl = fn(s, *vs)
    g = concl
    for a in reversed(ants):
        g = Imp(a, g)
    return g, [v for v, c in zip(vs, sorts) if c == 'E'], [v for v, c in zip(vs, sorts) if c == 'H']


def is_frame(rule, goal):
    return all(t[1][:-1] not in rule.upd for t in terms_of(goal, set())
               if t[0] == 'f' and t[1].endswith("'"))


def used(p, acc):
    if p.rule in SCHEMAS:
        acc.add(p.rule)
    for q in p.prem:
        used(q, acc)
    return acc


def smallest(rule, ev, hv, facts, goal, **kw):
    """A derivation from as few invariant clauses as possible."""
    for k in range(len(facts) + 1):
        for sub in combinations(facts, k):
            _id[0] = 0
            p = solve(setup(rule, ev, hv, sub, goal, **kw), goal)
            if p:
                return p
    return None


def preservation():
    out = []
    for name, _, _ in CLAUSES:
        goal, ev, hv = clause_goal(name, S_)
        for rule in RULES:
            p = smallest(rule, ev, hv, PRES_FACTS[name].split(), goal)
            out.append((name, rule.name, p, is_frame(rule, goal)))
    return out


def initial():
    out = []
    for name, _, _ in CLAUSES:
        goal, ev, hv = clause_goal(name, S)
        out.append((name, smallest(None, ev, hv, [], goal, init=True)))
    return out


# the step properties of ADM.v: name, facts handed to act, variables, statement
STEP = [
    ('A4_RevokedBeforeEnter', 'B2', [X], [Y],
     Imp(neq(S.cx(Y), Enc(X)), Imp(eq(S_.cx(Y), Enc(X)), All(Z, 'H', eq(S.pm(X, Z), pNONE))))),
    ('A7_ShareStateIntegrity', 'S1 S2', [X], [],
     Imp(Or(neq(S_.sh(X), S.sh(X)), neq(S_.mt(X), S.mt(X))), Or(
         And(eq(S.st(X), INVALID), eq(S_.st(X), FRESH)),
         Ex(HB, 'H', And(eq(S.cx(HB), Enc(X)), eq(S_.cx(HB), Host))),
         And(eq(S.ph(X), VALIDATING), Ex(HB, 'H', And(eq(S.cx(HB), Sm(X)), eq(S_.cx(HB), Enc(X))))),
         And(eq(S.st(X), DESTROYING), eq(S_.st(X), INVALID))))),
    ('A9_NoSecretAtRelease', 'Z', [X], [],
     Imp(eq(S.st(X), DESTROYING), Imp(eq(S_.st(X), INVALID), eq(S.sec(X), ff)))),
    ('B5_EnterOnlyViaValidation', 'S1 S2 D2b', [X], [Y],
     Imp(neq(S.cx(Y), Enc(X)), Imp(eq(S_.cx(Y), Enc(X)), And(
         eq(S.ph(X), VALIDATING), Imp(eq(S.sh(X), sRW), WF(S.mm(X), S.sm(X))))))),
    ('D1_CallerRestriction', 'S1 S2', [X], [],
     And(Imp(eq(S.st(X), RUNNING), Imp(eq(S_.st(X), STOPPED),
                                       Ex(HB, 'H', And(eq(S.cx(HB), Enc(X)), eq(S_.cx(HB), Host))))),
         Imp(Or(eq(S.st(X), FRESH), eq(S.st(X), STOPPED)), Imp(
             eq(S_.st(X), RUNNING), Ex(HB, 'H', And(eq(S.cx(HB), Host), eq(S_.cx(HB), Sm(X)))))))),
]


def step_properties():
    out = []
    for name, facts, ev, hv, goal in STEP:
        for rule in RULES:
            out.append((name, rule.name, smallest(rule, ev, hv, facts.split(), goal)))
    return out


H1, H2 = V('h1'), V('h2')
PE, SE = S.pm(E0, H0), S.sh(E0)
LEQ = [Fact('def', [], P('leq', a, b)) for a, b in
       [(pNONE, pNONE), (pNONE, pR), (pNONE, pRW), (pR, pR), (pR, pRW), (pRW, pRW)]]
HP = [Fact('def', [], eq(F('hp', a), b)) for a, b in [(sNONE, pNONE), (sRONLY, pR), (sRW, pRW)]]
ADV = Or(eq(S.cx(H0), Host), Ex(EB, 'E', And(neq(EB, E0), eq(S.cx(H0), Enc(EB)))))
# the state properties of ADM.v that have a proof script of their own:
# name, clauses and lemmas used, hart variables, statement, case analyses
STATE = [
    ('running_of_ctx', 'S1 S2', [H0],
     Imp(Or(eq(S.cx(H0), Enc(E0)), eq(S.cx(H0), Sm(E0))), And(eq(S.st(E0), RUNNING), eq(S.rn(E0), Some(H0)))),
     {}),
    ('running_of_phase', 'S4', [],
     Imp(Or(eq(S.ph(E0), REVOKING), eq(S.ph(E0), VALIDATING)), eq(S.st(E0), RUNNING)),
     dict(axioms=[(dec(S.st(E0), RUNNING), 'dec')])),
    ('A2_ExclusiveWhileRunning', 'A2', [H0],
     Imp(eq(S.cx(H0), Enc(E0)), And(eq(S.pm(E0, H0), pRW),
                                    All(HB, 'H', Imp(neq(HB, H0), eq(S.pm(E0, HB), pNONE))))), {}),
    ('A3_OnlyExplicitGrant', 'A3', [H0],
     Imp(neq(S.cx(H0), Enc(E0)), Imp(neq(S.st(E0), INVALID), P('leq', PE, F('hp', SE)))),
     dict(axioms=[(Or(eq(PE, pNONE), eq(PE, pR), eq(PE, pRW)), 'exh'), (exh(SE), 'exh')], extra=LEQ + HP)),
    ('A6_IsolatedWhenNotShared', 'D2c A3', [H0],
     Imp(eq(SE, sNONE), Imp(neq(PE, pNONE), eq(S.cx(H0), Enc(E0)))),
     dict(axioms=[(dec(S.cx(H0), Enc(E0)), 'dec'), (dec(S.st(E0), INVALID), 'dec')])),
    ('E1_NoUnintendedDisclosure', 'E1', [H0], Imp(eq(S.sec(E0), tt), Imp(ADV, eq(PE, pNONE))), {}),
    ('B1_ValidatedBeforeRun', 'Dirty S5', [H0], Imp(eq(S.cx(H0), Enc(E0)), eq(S.dt(E0), ff)),
     dict(axioms=[(Or(eq(S.dt(E0), tt), eq(S.dt(E0), ff)), 'exh')])),
    ('B3_FailClosed', 'B3 running_of_ctx', [],
     Imp(eq(S.dd(E0), tt), And(Or(eq(S.st(E0), DESTROYING), eq(S.st(E0), INVALID)),
                               All(HB, 'H', neq(S.cx(HB), Enc(E0))))), {}),
    ('B4_FreshMetadata', 'D2b running_of_phase', [],
     Imp(eq(S.ph(E0), VALIDATING), Imp(eq(SE, sRW), eq(S.mt(E0), S.sm(E0)))), {}),
    ('B5_FreshNeedsValidation', 'B5', [],
     Imp(eq(S.st(E0), FRESH), And(eq(SE, sRW), eq(S.dt(E0), tt))), {}),
    ('E2_ContentsPreserved', 'E2 S2 S3', [],
     Imp(Or(eq(S.st(E0), STOPPED), eq(S.st(E0), RUNNING)), Imp(Keep(S.ls(E0)), Imp(
         All(HB, 'H', neq(S.cx(HB), Enc(E0))), eq(S.mm(E0), S.sn(E0))))),
     dict(axioms=[(Or(Ex(HB, 'H', eq(S.rn(E0), Some(HB))), eq(S.rn(E0), botH)), 'exh')])),
    ('D2a_RunningConsistent', 'S1 S2 S5', [H0],
     Imp(eq(S.cx(H0), Enc(E0)), And(eq(S.st(E0), RUNNING), eq(S.ph(E0), IDLE), eq(S.rn(E0), Some(H0)),
                                    eq(SE, sNONE), eq(S.mt(E0), botM))), {}),
    ('D2d_SingleRunner', 'S1', [H1, H2],
     Imp(Or(eq(S.cx(H1), Enc(E0)), eq(S.cx(H1), Sm(E0))),
         Imp(Or(eq(S.cx(H2), Enc(E0)), eq(S.cx(H2), Sm(E0))), eq(H1, H2))), {}),
]


def state_properties():
    out = []
    for name, facts, hv, goal, kw in STATE:
        _id[0] = 0
        out.append((name, solve(setup(None, [E0], hv, facts.split(), goal, **kw), goal)))
    return out


def step_cases():
    """step_cases of ADMRegions.v: the steps other than Create and
    DestroyFree keep the set of existing enclaves."""
    goal = And(Imp(eq(S.st(X), INVALID), eq(S_.st(X), INVALID)),
               Imp(eq(S_.st(X), INVALID), eq(S.st(X), INVALID)))
    return [(r.name, smallest(r, [X], [], ['running_of_phase'], goal))
            for r in RULES if r.name not in ('Create', 'DestroyFree')]


X1, X2 = V('x1'), V('x2')
C1_GOALS = [
    ([X], Imp(In(AV, S_.epm(X)), Imp(In(AV, S_.adm(X)), BOT))),
    ([X], Imp(Prot(S_, X, AV), Imp(P('SMR', AV), BOT))),
    ([X1, X2], Imp(neq(X1, X2), Imp(Prot(S_, X1, AV), Imp(Prot(S_, X2, AV), BOT)))),
]
LIFE_GOALS = [
    ([X], Imp(eq(S_.st(X), INVALID), And(Empty(S_.epm(X)), Empty(S_.adm(X))))),
    ([X], Imp(And(Empty(S_.epm(X)), Empty(S_.adm(X))), eq(S_.st(X), INVALID))),
]


def unprime(f):
    if f[0] == 'eq':
        return ('eq',) + tuple(unprime(t) for t in f[1:])
    if f[0] == 'f':
        return ('f', f[1].rstrip("'")) + tuple(unprime(t) for t in f[2:])
    if f[0] in ('c', 'v'):
        return f
    if f[0] in ('all', 'ex'):
        return f[:3] + (unprime(f[3]),)
    return (f[0],) + tuple(unprime(g) for g in f[1:])


def regions():
    """C1_reachable and lifecycle_reachable: the initial pair and the three
    transitions of stepR."""
    out = []
    for title, goals, facts, av in [('C1_reachable', C1_GOALS, ['C1', 'empty'], [AV]),
                                    ('lifecycle_reachable', LIFE_GOALS, ['IH', 'empty'], [])]:
        for i, (ev, goal) in enumerate(goals):
            _id[0] = 0
            g0 = unprime(goal)
            out.append((title, i, 'RR_init', solve(setup(None, ev, [], ['empty'], g0, avars=av, init=True), g0)))
            for rule in RRULES:
                _id[0] = 0
                out.append((title, i, rule.name, solve(setup(rule, ev, [], facts, goal, avars=av), goal)))
    return out


def match_perms(h, a, p):
    return Or(Ex(EB, 'E', And(In(a, S.epm(EB)), eq(p, F('EP', h, EB)))),
              Ex(EB, 'E', And(In(a, S.adm(EB)), eq(p, F('MPc', S.pm(EB, h))))))


MATCH = match_perms(H0, AV, PV)
DERIVED = [
    ('SMNotShadowed', Imp(P('SMR', AV), Not(MATCH))),
    ('EPMIsolated', Imp(In(AV, S.epm(E0)), And(Imp(MATCH, eq(PV, F('EP', H0, E0))),
                                               Imp(eq(PV, F('EP', H0, E0)), MATCH)))),
    ('ADMPermFaithful', Imp(In(AV, S.adm(E0)), And(Imp(MATCH, eq(PV, F('MPc', S.pm(E0, H0)))),
                                                   Imp(eq(PV, F('MPc', S.pm(E0, H0))), MATCH)))),
]


def derived():
    out = []
    for name, goal in DERIVED:
        _id[0] = 0
        out.append((name, solve(setup(None, [E0], [H0], ['C1'], goal, avars=[AV]), goal)))
    return out


# --- Typst ----------------------------------------------------------------------

TN = {'sNONE': 'NONE', 'sRONLY': 'RONLY', 'sRW': 'RW', 'pNONE': 'NONE', 'pR': 'Rd', 'pRW': 'RW',
      'botH': 'bot', 'botM': 'bot', 'm0': 'm_0', 'zero': '0', 'WF': 'WFo', 'EMPTY': 'emptyset',
      'Re': 'R_e', 'Ra': 'R_a', 'e1': 'e_1', 'e2': 'e_2', 'h1': 'h_1', 'h2': 'h_2', 'x1': 'x_1', 'x2': 'x_2'}
TL = {'st': 5, 'sh': 5, 'mt': 4, 'ph': 5, 'rn': 6, 'cx': 3, 'pm': 3, 'mm': 3, 'sec': 6, 'ls': 8,
      'dt': 5, 'sm': 8, 'dd': 4, 'sn': 4, 'WF': 2, 'EP': 7, 'hp': 8}
RL = {'→I': '$->$I', '→E': '$->$E', '∧I': '$and$I', '∧E': '$and$E', '∨I': '$or$I', '∨E': '$or$E',
      '¬I': '$not$I', '¬E': '$not$E', '⊥E': '$bot$E', '∀I': '$forall$I', '∀E': '$forall$E',
      '∃I': '$exists$I', '∃E': '$exists$E', '=': '$=$',
      'running_of_ctx': 'ctx', 'running_of_phase': 'phase'}


def tt_(t):
    """Typst source and estimated width (in characters) of a term."""
    if t[0] in 'cv':
        n = TN.get(t[1], t[1])
        return n, len(n.replace('_', ''))
    f, args = t[1], [tt_(a) for a in t[2:]]
    if f in ('Some', 'MPc'):
        return args[0]
    if f == 'in':
        return '%s in %s' % (args[0][0], args[1][0]), args[0][1] + args[1][1] + 3
    if f == 'SMR':
        return '%s in SMR' % args[0][0], args[0][1] + 11
    if f == 'leq':
        return '%s <= %s' % (args[0][0], args[1][0]), args[0][1] + args[1][1] + 3
    base = f.rstrip("'")
    return ('%s%s(%s)' % (TN.get(base, base), "'" * (len(f) - len(base)), ', '.join(a[0] for a in args)),
            TL.get(base, len(base)) + 2 + sum(a[1] + 2 for a in args))


def abbrev(f):
    """Keep(k), R = {} and a in Prot(x) for the formulas they stand for."""
    if f[0] == 'or' and len(f) == 5 and f[1][0] == 'eq' and f == Keep(f[1][1]):
        t, w = tt_(f[1][1])
        return 'Keep(%s)' % t, w + 6
    if f[0] == 'all' and f[3][0] == 'not' and f[3][1][0] == 'eq' and f[3][1][1][:2] == ('f', 'in') \
            and f == Empty(f[3][1][1][3]):
        t, w = tt_(f[3][1][1][3])
        return '%s = emptyset' % t, w + 4
    if f[0] == 'or' and len(f) == 3 and all(g[0] == 'eq' and g[2] == TOP and g[1][1] == 'in' for g in f[1:]):
        (a, r1), (b, r2) = f[1][1][2:], f[2][1][2:]
        if a == b and r1[0] == 'f' and r2[0] == 'f' and r1[2:] == r2[2:] and \
                (r1[1].rstrip("'"), r2[1].rstrip("'")) == ('epm', 'adm') and r1[1][3:] == r2[1][3:]:
            (ta, wa), (tx, wx) = tt_(a), tt_(r1[2])
            return '%s in Prot%s(%s)' % (ta, r1[1][3:], tx), wa + wx + 9
        if a == b:
            (ta, wa), (t1, w1), (t2, w2) = tt_(a), tt_(r1), tt_(r2)
            return '%s in %s union %s' % (ta, t1, t2), wa + w1 + w2 + 6
    return None


def ft(f, par=0):
    """Typst source and width of a formula; par: 1 inside or / not, 2 inside and."""
    k = f[0]
    if f in ABBR:
        return ABBR[f]
    ab = abbrev(f)
    if ab:
        return ab
    if k == 'eq':
        if f[2] == TOP:
            return tt_(f[1])
        (a, wa), (b, wb) = tt_(f[1]), tt_(f[2])
        return '%s = %s' % (a, b), wa + wb + 3
    if k == 'not':
        if f[1][0] == 'eq' and f[1][2] != TOP:
            (a, wa), (b, wb) = tt_(f[1][1]), tt_(f[1][2])
            return '%s != %s' % (a, b), wa + wb + 3
        if f[1][0] == 'eq' and f[1][1][1] in ('in', 'SMR'):
            a, w = tt_(f[1][1])
            return a.replace(' in ', ' in.not ', 1), w
        a, w = ft(f[1], 3)
        return 'not ' + a, w + 1
    if k == 'bot':
        return 'bot', 1
    if k in ('all', 'ex'):
        v, _ = tt_(f[1])
        a, w = ft(f[3], 0)
        s, w = '%s %s. thick %s' % ('forall' if k == 'all' else 'exists', v, a), w + 4
        return ('(%s)' % s, w + 2) if par else (s, w)
    if k == 'imp':
        (a, wa), (b, wb) = ft(f[1], 1), ft(f[2], 0)
        s, w = '%s -> %s' % (a, b), wa + wb + 3
        return ('(%s)' % s, w + 2) if par else (s, w)
    lvl = 2 if k == 'and' else 1
    parts = [ft(g, lvl) for g in f[1:]]
    s = (' and ' if k == 'and' else ' or ').join(p[0] for p in parts)
    w = sum(p[1] for p in parts) + 3 * (len(parts) - 1)
    if par and par != lvl:
        return '(%s)' % s, w + 2
    return s, w


LIMIT = 225
ABBR = {}


def label(p, num):
    l = RL.get(p.rule, p.rule)
    if p.dis:
        l += '#super[%s]' % ','.join(str(num[i]) for i in p.dis)
    return l


class Layout:
    """Typst source of a derivation.  A subderivation that occurs more than
    once, or that does not fit in the width, is shown separately."""

    def __init__(self, p, th=70):
        self.num, self.out, self.k, self.wide, self.th = {}, [], 0, 0, th
        self.keys, self.memo, self.count, self.shared = {}, {}, {}, {}
        self.number(p)
        self.scan(p)
        self.root = p
        self.names(p)
        src, w = self.go(p)
        self.out.append((None, src))
        self.wide = max(self.wide, w)
        ABBR.clear()

    def names(self, p):
        """Phi_i for a long formula that occurs several times."""
        cnt, seen = {}, set()

        def walk(q):
            if id(q) in seen:
                return
            seen.add(id(q))
            cnt[q.concl] = cnt.get(q.concl, 0) + 1
            for r in q.prem:
                walk(r)
        walk(p)
        ABBR.clear()
        self.legend = []
        for f in sorted([f for f, n in cnt.items() if n >= 3 and ft(f)[1] >= self.th], key=lambda f: ft(f)[1]):
            if ft(f)[1] < self.th:
                continue
            self.legend.append('$Phi_%d = %s$' % (len(self.legend) + 1, ft(f)[0]))
            ABBR[f] = ('Phi_%d' % len(self.legend), 2)

    def number(self, p):
        for q in p.prem:
            self.number(q)
        for i in p.dis:
            self.num.setdefault(i, len(self.num) + 1)

    def key(self, p):
        if id(p) not in self.memo:
            k = (p.rule, p.concl, tuple(self.key(q) for q in p.prem), p.hyp, tuple(p.dis))
            self.memo[id(p)] = self.keys.setdefault(k, len(self.keys))
        return self.memo[id(p)]

    def scan(self, p):
        k = self.key(p)
        self.count[k] = self.count.get(k, 0) + 1
        if self.count[k] == 1:
            for q in p.prem:
                self.scan(q)

    def go(self, p):
        if p is self.root and p.concl in ABBR:
            a = ABBR.pop(p.concl)
            f, w = ft(p.concl)
            ABBR[p.concl] = a
        else:
            f, w = ft(p.concl)
        if p.hyp is not None:
            return '(c: $[%s]^%d$)' % (f, self.num[p.hyp]), w + 3
        k = self.key(p)
        if k in self.shared:
            return '(c: $%s$, r: %d)' % (f, self.shared[k]), w + 1
        lab = label(p, self.num)
        lw = 2 + len(RL.get(p.rule, p.rule)) // 2 + len(p.dis)
        subs = [self.go(q) for q in p.prem]
        while len(subs) > 0 and sum(s[1] for s in subs) + 2 * (len(subs) - 1) + lw > LIMIT:
            gain = [subs[j][1] - ft(q.concl)[1] - 1 for j, q in enumerate(p.prem)]
            i = max(range(len(subs)), key=lambda j: gain[j])
            if gain[i] <= 3:
                break
            cf, cw = ft(p.prem[i].concl)
            self.k += 1
            self.out.append((self.k, subs[i][0]))
            self.wide = max(self.wide, subs[i][1])
            subs[i] = ('(c: $%s$, r: %d)' % (cf, self.k), cw + 1)
        tw = sum(s[1] for s in subs) + 2 * max(len(subs) - 1, 0)
        src = '(c: $%s$, n: [%s], p: (%s))' % (f, lab, ''.join(s[0] + ', ' for s in subs))
        if self.count[k] > 1 and p.size >= 5:
            self.k += 1
            self.shared[k] = self.k
            self.out.append((self.k, src))
            self.wide = max(self.wide, max(w, tw) + lw)
            return '(c: $%s$, r: %d)' % (f, self.k), w + 1
        return src, max(w, tw) + lw


# --- check of a derivation ------------------------------------------------------

def head(t):
    return t if t[0] == 'c' else t[1] if t[0] == 'f' and t[1] in INJ else None


def same_mod(f, g, cc):
    """f is g up to terms that cc makes equal."""
    if f[0] != g[0] or len(f) != len(g):
        return False
    if f[0] in ('all', 'ex'):
        return f[1:3] == g[1:3] and same_mod(f[3], g[3], cc)
    if f[0] == 'eq':
        return all(cc.same(a, b) for a, b in zip(f[1:], g[1:]))
    return all(same_mod(a, b, cc) for a, b in zip(f[1:], g[1:]))


def verify(p, top=True):
    """Every inference of p is an instance of its rule, and no assumption is
    left open.  The rules of the model (pre, post, a clause, ...) are leaves."""
    for q in p.prem:
        verify(q, False)
    r, c, cs = p.rule, p.concl, [q.concl for q in p.prem]

    def leaves(q, i, acc):
        if q.hyp == i:
            acc.add(q.concl)
        for x in q.prem:
            if i in x.free:
                leaves(x, i, acc)
        return acc

    def dis(q, i, f):
        return leaves(q, i, set()) <= {f}

    def fresh(v, q, but):
        fs = ([] if but is None else [c]) + [x for i in q.free - {but} for x in leaves(q, i, set())]
        return all(v not in terms_of(f, set()) for f in fs)

    def inst(body, v, f):
        return any(subst(body, v, t) == f for t in terms_of(f, {v}))

    if p.hyp is not None:
        ok = True
    elif r == '→I':
        ok = c[0] == 'imp' and cs == [c[2]] and dis(p.prem[0], p.dis[0], c[1])
    elif r == '¬I':
        ok = c[0] == 'not' and cs == [BOT] and dis(p.prem[0], p.dis[0], c[1])
    elif r == '→E':
        ok = cs[0] == ('imp', cs[1], c)
    elif r == '¬E':
        ok = c == BOT and cs[0] == ('not', cs[1])
    elif r == '⊥E':
        ok = cs == [BOT]
    elif r == '∧I':
        ok = c == ('and',) + tuple(cs)
    elif r == '∧E':
        ok = cs[0][0] == 'and' and c in cs[0][1:]
    elif r == '∨I':
        ok = c[0] == 'or' and cs[0] in c[1:]
    elif r == '∨E':
        d = cs[0]
        ok = d[0] == 'or' and len(d) == len(cs) == len(p.dis) + 1 and all(x == c for x in cs[1:]) and \
            all(dis(q, i, f) for q, i, f in zip(p.prem[1:], p.dis, d[1:]))
    elif r == '∀I':
        ok = c[0] == 'all' and cs == [c[3]] and fresh(c[1], p.prem[0], None)
    elif r == '∀E':
        ok = cs[0][0] == 'all' and inst(cs[0][3], cs[0][1], c)
    elif r == '∃I':
        ok = c[0] == 'ex' and inst(c[3], c[1], cs[0])
    elif r == '∃E':
        a, q, i = cs[0], p.prem[1], p.dis[0]
        hs = leaves(q, i, set())
        vs = [v for v in terms_of(next(iter(hs)), set()) if v[0] == 'v'] if hs else []
        ok = a[0] == 'ex' and cs[1] == c and len(hs) <= 1 and (not hs or any(
            subst(a[3], a[1], v) in hs and fresh(v, q, i) and v not in terms_of(a, set()) for v in vs))
    elif r == '=':
        eqs = [(f[1], f[2]) for f in cs if f[0] == 'eq']
        if c[0] == 'eq':
            ok = CC(eqs, [c[1], c[2]]).same(c[1], c[2])
        else:
            cc = CC([(f[1], f[2]) for f in cs[1:]], terms_of(c, terms_of(cs[0], set())))
            ok = all(f[0] == 'eq' for f in cs[1:]) and same_mod(c, cs[0], cc)
    elif r == 'dis':
        a, b = head(cs[0][1]), head(cs[0][2])
        ok = c == BOT and a is not None and b is not None and a != b
    else:
        ok = r in SCHEMAS or r in ('pre', 'post', 'init', 'def', 'dec', 'exh')
    if not ok or (top and p.free):
        raise AssertionError('bad inference %s: %s' % (r, ft(c)[0]))
    return p



def figure(p):
    verify(p)
    for th in (70, 50, 35, 25):
        lay = Layout(p, th)
        if lay.wide <= LIMIT:
            break
    out = ['#legend[%s]' % l for l in lay.legend]
    for k, src in lay.out:
        out.append('#tree(%s%s)' % ('' if k is None else 'head: $Pi_%d$, ' % k, src))
    return '\n'.join(out)


def main_typ():
    pr = print
    pr('// Generated by prooftree.py; do not edit.')
    pr('#import "rocq.typ": *')
    pr('#import "prooftree.typ": legend, tree')

    pr('#let initial = [')
    for name, p in initial():
        pr('=== %s\n%s' % (name, figure(p)))
    pr(']')

    pr('#let preservation(t) = [')
    res = preservation()
    prev = {}
    for name, rn, p, frame in res:
        if rn == 'Create':
            pr('== %s (`p_%s`)' % (name, name.lower()))
            pr('#t([フレーム：], [Frame: ])%s' % ', '.join(r for n, r, _, fr in res if n == name and fr))
        if frame:
            continue
        pr('=== %s, %s' % (name, rn))
        src = figure(p)
        if rn == 'Exit' and src.replace('exit', 'invol') == prev.get('StopInvoluntary'):
            pr('#t([StopInvoluntary の図で $invol$ を $exit$ に読み替えたもの。], '
               '[The figure of StopInvoluntary with $exit$ for $invol$.])')
        else:
            pr(src)
        prev[rn] = src
    pr(']')

    pr('#let steps(t) = [')
    for name, rn, p in step_properties():
        if rn == 'Create':
            pr('== %s (`%s`)' % (name.split('_')[0], name))
        pr('=== %s, %s\n%s' % (name.split('_')[0], rn, figure(p)))
    pr(']')

    pr('#let states = (')
    for name, p in state_properties():
        pr('  %s: [%s],' % (name, figure(p)))
    pr(')')

    pr('#let stepcases = [')
    for rn, p in step_cases():
        pr('=== %s\n%s' % (rn, figure(p)))
    pr(']')

    pr('#let regions = (')
    cur = None
    for title, i, rn, p in regions():
        if (title, i) != cur:
            if cur:
                pr('  ],')
            cur = (title, i)
            pr('  %s_%d: [' % (title, i + 1))
        pr('*%s* \\ %s\n' % (rn, figure(p)))
    pr('  ],')
    for name, p in derived():
        pr('  %s: [%s],' % (name, figure(p)))
    pr(')')


# --- Rocq -----------------------------------------------------------------------

RC = {'sNONE': 'SH_NONE', 'sRONLY': 'SH_RONLY', 'sRW': 'SH_RW', 'pNONE': 'P_NONE', 'pR': 'P_R', 'pRW': 'P_RW',
      'botH': 'None', 'botM': 'None', 'tt': 'true', 'ff': 'false', 'knone': 'K_none',
      'sharenone': 'K_share_none', 'sharero': 'K_share_ro', 'sharerw': 'K_share_rw', 'invol': 'K_invol',
      'exit': 'K_exit', 'EMPTY': 'emptyR'}
RF = {'st': 'state', 'sh': 'share', 'mt': 'meta', 'ph': 'phase', 'rn': 'runner', 'cx': 'ctx', 'pm': 'pmp',
      'mm': 'mem', 'sec': 'secret', 'ls': 'lastStop', 'dt': 'dirty', 'sm': 'sentMeta', 'dd': 'dead',
      'sn': 'snap'}
RV = {'s': 'sh', 'zero': 'Zero'}
# parameters and post-state of the constructors of step
RSTEP = {
    'Create': ('h e d m0', 'do_create s e d m0'), 'RunBegin': ('h e', 'do_begin s h e VALIDATING'),
    'ResumeBegin': ('h e', 'do_begin s h e REVOKING'), 'RevokeStep': ('h e', 'do_revoke_step s h e'),
    'RevokeDone': ('e', 'do_revoke_done s e'), 'ValidatePass': ('e h', 'do_validate_pass s e h'),
    'ValidateFail': ('e h', 'do_validate_fail s e h'),
    'StopShare': ('h e sh m', 'do_stop s h e sh m (kind_of sh)'),
    'StopInvoluntary': ('h e', 'do_stop s h e SH_NONE None K_invol'),
    'Exit': ('h e', 'do_stop s h e SH_NONE None K_exit'), 'DestroyBegin': ('h e', 'do_destroy_begin s e'),
    'DestroyZero': ('e', 'do_destroy_zero s e'), 'DestroyFree': ('e', 'do_destroy_free s e'),
    'EncWrite': ('h e d', 'do_enc_write s e d'), 'AdvWrite': ('h e d', 'do_adv_write s e d'),
}
LTAC = '''
(* tactics of the figures: pre, post, and the second form of = *)
Ltac figpre := first [ assumption | tauto | (unfold Adversarial in *; assumption) ].
Ltac figpost :=
  repeat match goal with v := _ |- _ => subst v end;
  UNFOLD; cases; try subst;
  repeat match goal with Q : share _ _ = _ |- _ => rewrite Q end;
  cbn; cases; cbn; first [ congruence | reflexivity ].
Ltac figeq Q :=
  first [ congruence
        | solve [ repeat match goal with E : _ = _ |- _ => rewrite <- E in Q end; exact Q ]
        | solve [ repeat match goal with E : _ = _ |- _ => rewrite E in Q end; exact Q ] ].
Ltac figasm := first [ eassumption | tauto | (unfold Adversarial in *; eassumption) ].
'''
LTAC_ADM = LTAC.replace('UNFOLD', 'unfold_all; unfold updE, updH in *')
LTAC_REG = '''
Ltac cases :=
  repeat match goal with
  | |- context [eid_eq_dec ?a ?b] => destruct (eid_eq_dec a b); try subst
  end.
''' + LTAC.replace('UNFOLD', 'unfold ADM.do_create, ADM.do_destroy_free, ADM.updE, setR, emptyR in *; cbn in *') \
          .replace('repeat match goal with Q : share _ _ = _ |- _ => rewrite Q end;\n  ', '') \
          .replace(' | (unfold Adversarial in *; assumption)', '').replace(' | (unfold Adversarial in *; eassumption)', '')


class Rq:
    """Rocq text of terms, formulas and derivations.  cl maps the name of a
    clause or lemma to a function from (index, arguments) to its term."""

    def __init__(self, cl, s='s', s1="s'", r='r', r1="r'"):
        self.cl, self.s, self.s1, self.r, self.r1, self.n = cl, s, s1, r, r1, 0

    def t(self, t):
        if t[0] == 'c':
            return RC.get(t[1], t[1])
        if t[0] == 'v':
            return RV.get(t[1], t[1])
        f, a = t[1], [self.t(x) for x in t[2:]]
        if f in ('Enc', 'Sm', 'Some'):
            return '(%s %s)' % (f, a[0])
        if f == 'MPc':
            return '(MP %s)' % a[0]
        if f == 'EP':
            return '(EpmPerm %s %s %s)' % (self.s, a[0], a[1])
        if f == 'hp':
            return '(HostPerm %s)' % a[0]
        base = f.rstrip("'")
        if base in ('epm', 'adm'):
            return '(%s %s %s)' % (base, self.r1 if f != base else self.r, a[0])
        return '(%s %s %s)' % (RF[base], self.s1 if f != base else self.s, ' '.join(a))

    def f(self, f):
        k = f[0]
        if k == 'eq':
            if f[2] == TOP:
                g, a = f[1][1], [self.t(x) for x in f[1][2:]]
                if g == 'in':
                    return '(%s %s)' % (a[1], a[0])
                return '(%s %s)' % ({'SMR': 'SMRegion', 'leq': 'PermLeq', 'WF': 'WFo'}[g], ' '.join(a))
            return '(%s = %s)' % (self.t(f[1]), self.t(f[2]))
        if k == 'not':
            return '(~ %s)' % self.f(f[1])
        if k == 'bot':
            return 'False'
        if k in ('and', 'or'):
            s = self.f(f[-1])
            for g in reversed(f[1:-1]):
                s = '(%s %s %s)' % (self.f(g), '/\\' if k == 'and' else '\\/', s)
            return s
        if k == 'imp':
            return '(%s -> %s)' % (self.f(f[1]), self.f(f[2]))
        return '(%s %s, %s)' % ('forall' if k == 'all' else 'exists', self.t(f[1]), self.f(f[3]))

    def have(self, p, out):
        """Name of a hypothesis that states the conclusion of p."""
        if p.hyp is not None:
            return 'H%d' % p.hyp
        self.n += 1
        q = 'Q%d' % self.n
        out.append('assert (%s : %s).' % (q, self.f(p.concl)))
        out.append('{')
        self.go(p, out)
        out.append('}')
        return q

    def copy(self, p, out):
        """As have, for a hypothesis that destruct consumes."""
        q = self.have(p, out)
        if p.hyp is not None:
            self.n += 1
            out.append('pose proof %s as Q%d.' % (q, self.n))
            q = 'Q%d' % self.n
        return q

    def block(self, p, out):
        out.append('{')
        self.go(p, out)
        out.append('}')

    def go(self, p, out):
        """Tactics that prove the conclusion of p, one step for each inference."""
        r, c, ps = p.rule, p.concl, p.prem
        if p.hyp is not None:
            out.append('exact H%d.' % p.hyp)
        elif r in ('→I', '¬I'):
            out.append('intro H%d.' % p.dis[0])
            self.go(ps[0], out)
        elif r in ('→E', '¬E'):
            out.append('refine (%s _).' % self.have(ps[0], out))
            self.go(ps[1], out)
        elif r == '⊥E':
            out.append('exfalso.')
            self.go(ps[0], out)
        elif r == '∧I':
            for q in ps[:-1]:
                out.append('split.')
                self.block(q, out)
            self.go(ps[-1], out)
        elif r == '∧E':
            q, a = self.have(ps[0], out), ps[0].concl
            i = a.index(c) - 1
            t = q
            for _ in range(i):
                t = '(proj2 %s)' % t
            out.append('exact (%s).' % (t if i == len(a) - 2 else 'proj1 ' + t))
        elif r == '∨I':
            i = c.index(ps[0].concl) - 1
            out.append('right. ' * i + ('left.' if i < len(c) - 2 else ''))
            self.go(ps[0], out)
        elif r == '∨E':
            pat = 'H%d' % p.dis[-1]
            for i in reversed(p.dis[:-1]):
                pat = 'H%d | %s' % (i, pat) if pat.startswith('H') and '|' not in pat else 'H%d | [%s]' % (i, pat)
            out.append('destruct %s as [%s].' % (self.copy(ps[0], out), pat))
            for q in ps[1:]:
                self.block(q, out)
        elif r == '∀I':
            out.append('intro %s.' % self.t(c[1]))
            self.go(ps[0], out)
        elif r == '∀E':
            a = ps[0].concl
            w = next(t for t in terms_of(c, {a[1]}) if subst(a[3], a[1], t) == c)
            out.append('exact (%s %s).' % (self.have(ps[0], out), self.t(w)))
        elif r == '∃I':
            w = next(t for t in terms_of(ps[0].concl, {c[1]}) if subst(c[3], c[1], t) == ps[0].concl)
            out.append('exists %s.' % self.t(w))
            self.go(ps[0], out)
        elif r == '∃E':
            a, i = ps[0].concl, p.dis[0]
            h = next(x.concl for x in walk(ps[1]) if x.hyp == i)
            v = next(t for t in terms_of(h, set()) if t[0] == 'v' and subst(a[3], a[1], t) == h)
            out.append('destruct %s as [%s H%d].' % (self.copy(ps[0], out), self.t(v), i))
            self.go(ps[1], out)
        elif r == '=':
            if not ps:
                out.append('reflexivity.')
                return
            qs = [self.have(q, out) for q in ps]
            out.append('clear - %s.' % ' '.join(dict.fromkeys(qs)))
            atom = c[0] == 'eq' or (c[0] == 'not' and c[1][0] == 'eq')
            out.append('congruence.' if atom else 'figeq %s.' % qs[0])
        elif r == 'dis':
            out.append('discriminate %s.' % self.have(ps[0], out))
        elif r == 'pre':
            if p.info:
                self.clause(p, out)
            else:
                out.append('figpre.')
        elif r == 'post':
            for q in ps:
                self.have(q, out)
            out.append('figpost.')
        elif r == 'init':
            out.append('reflexivity.')
        elif r == 'def' and not p.info:
            out.append('first [ reflexivity | exact I ].')
        elif r == 'dec':
            out.append(self.dec(c))
        elif r == 'exh':
            if c[1][0] == 'ex':
                out.append('destruct %s as [hq|]; [left; exists hq; reflexivity | right; reflexivity].'
                           % self.t(c[2][1]))
            else:
                out.append('destruct %s; auto.' % self.t(c[1][1]))
        else:
            self.clause(p, out)

    def clause(self, p, out):
        name, j, asg = p.info
        if name == 'empty':
            out.append('exact %s.' % self.have(p.prem[0], out))
            return
        out.append('refine (%s%s).' % (self.cl[name](j, [self.t(a) for a in asg]), ' _' * len(p.prem)))
        for q in p.prem:
            self.block(q, out)

    def dec(self, c):
        a, b = c[1][1], c[1][2]
        if a[0] == 'v' and b[0] == 'v':
            fn = 'eid_eq_dec' if a[1][0] in 'ex' else 'hart_eq_dec'
            return 'destruct (%s %s %s) as [Dq|Dq]; [left | right]; exact Dq.' % (fn, self.t(a), self.t(b))
        if b[0] == 'c':
            return 'destruct %s; first [ left; reflexivity | right; discriminate ].' % self.t(a)
        return ('destruct %s as [|eq|eq]; [ right; discriminate | destruct (eid_eq_dec eq %s) as [Dq|Dq]; '
                '[ left; congruence | right; congruence ] | right; discriminate ].'
                % (self.t(a), self.t(b[2])))

    def lemma(self, name, binders, stmt, intros, p, pre=()):
        """A lemma whose proof follows the derivation p."""
        self.n = 0
        out = ['Lemma %s %s :\n  %s.' % (name, binders, stmt), 'Proof.']
        if intros:
            out.append('intros %s.' % intros)
        out += pre
        self.go(p, out)
        out.append('Qed.\n')
        return '\n'.join(out)


def walk(p):
    yield p
    for q in p.prem:
        yield from walk(q)


def closed(rq, vs, goal):
    return rq.f(goal) if not vs else '(forall %s, %s)' % (' '.join(rq.t(v) for v in vs), rq.f(goal))


def inv_clause(hi):
    return {c[0]: (lambda n: lambda j, a: '(i_%s s %s %s)' % (n.lower(), hi, ' '.join(a)))(c[0]) for c in CLAUSES}


def case_lemma(prefix, name, rule, vs, goal, p, cl, hyps='(s : St) (HI : Inv s)'):
    """The lemma of one transition rule: its premises are hypotheses, and the
    primed values are those of its post-state."""
    params, post = RSTEP[rule.name]
    rq = Rq(cl, s1='(%s)' % post)
    gs = ''.join(' (G%d : %s)' % (i, rq.f(g)) for i, g in enumerate(rule.guards))
    stmt = closed(rq, vs, goal)
    rq.s1 = "s'"
    return rq.lemma('%s_%s_%s' % (prefix, name, rule.name), '%s %s%s' % (hyps, params, gs), stmt,
                    ' '.join(rq.t(v) for v in vs), p, ["set (s' := %s)." % post])


def assembled(prefix, name, vs, goal, hyps='(HI : Inv s)', args='s HI'):
    rq = Rq({})
    alts = ' | '.join('eapply (%s_%s_%s %s)' % (prefix, name, r.name, args) for r in RULES)
    return ('Lemma %ss_%s s s\' %s (Hst : step s s\') :\n  %s.\nProof.\n  destruct Hst; [ %s ]; figasm.\nQed.\n'
            % (prefix, name, hyps, closed(rq, vs, goal), alts))


def rocq_adm(src):
    stm = dict(re.findall(r'(?:Lemma|Theorem) (\w+) : (.*?)\nProof\.', src, re.S))
    cl = inv_clause('HI')
    out = [LTAC_ADM]
    names = [c[0] for c in CLAUSES]
    # the initial state
    for name, p in initial():
        goal, ev, hv = clause_goal(name, S)
        rq = Rq({}, s='init')
        out.append(rq.lemma('fig_init_' + name, '', closed(rq, ev + hv, goal),
                            ' '.join(rq.t(v) for v in ev + hv), p))
    out.append('Lemma fig_inv_init : Inv init.\nProof.\n  constructor; [ %s ].\nQed.\n'
               % ' | '.join('exact fig_init_' + n for n in names))
    # preservation
    for name, rn, p, _ in preservation():
        goal, ev, hv = clause_goal(name, S_)
        out.append(case_lemma('fig', name, RULE[rn], ev + hv, goal, p, cl))
        if rn == RULES[-1].name:
            out.append(assembled('fig', name, ev + hv, goal))
    out.append('Lemma fig_inv_step s s\' (HI : Inv s) (Hst : step s s\') : Inv s\'.\nProof.\n'
               '  constructor; [ %s ].\nQed.\n' % ' | '.join("exact (figs_%s s s' HI Hst)" % n for n in names))
    out.append('Theorem fig_inv_reachable : forall s, reachable s -> Inv s.\nProof.\n  induction 1.\n'
               '  - exact fig_inv_init.\n  - eapply fig_inv_step; eassumption.\nQed.\n')
    # step properties
    spec = {n: (ev, hv, goal) for n, _, ev, hv, goal in STEP}
    for name, rn, p in step_properties():
        ev, hv, goal = spec[name]
        out.append(case_lemma('figt', name, RULE[rn], ev + hv, goal, p, cl))
        if rn == RULES[-1].name:
            out.append(assembled('figt', name, ev + hv, goal))
            out.append('Lemma figc_%s s s\' (R : reachable s) (Hst : step s s\') :\n  %s\nProof.\n'
                       '  exact (figts_%s s s\' (fig_inv_reachable s R) Hst).\nQed.\n' % (name, stm[name], name))
    # state properties
    names = []
    out.append('Section FigState.\nVariable s : St.\nHypothesis R : reachable s.\n')
    cl = inv_clause('(fig_inv_reachable s R)')
    cl['running_of_ctx'] = lambda j, a: '(fig_running_of_ctx %s)' % ' '.join(a)
    cl['running_of_phase'] = lambda j, a: '(fig_running_of_phase %s)' % ' '.join(a)
    for (name, _, hv, goal, _), (_, p) in zip(STATE, state_properties()):
        rq = Rq(cl)
        vs = ' '.join(rq.t(v) for v in [E0] + hv)
        out.append(rq.lemma('fig_' + name, '', closed(rq, [E0] + hv, goal), vs, p))
        out.append('Lemma figc_%s : %s\nProof.\n  first [ exact fig_%s | unfold Adversarial; exact fig_%s ].\nQed.\n'
                   % (name, stm[name], name, name))
        names.append('figc_' + name)
    for name, k in [('A1_InitialProtection', 'a1'), ('A8_InvoluntaryStopSafe', 'a8'),
                    ('B2_NoAccessDuringValidation', 'b2'), ('D2b_MetaMatchesSent', 'd2b'),
                    ('D2c_InvalidConsistent', 'd2c')]:
        out.append('Lemma figc_%s : %s\nProof. exact (i_%s s (fig_inv_reachable s R)). Qed.\n' % (name, stm[name], k))
        names.append('figc_' + name)
    out.append('End FigState.\n')
    # step_cases of ADMRegions.v
    goal = And(Imp(eq(S.st(X), INVALID), eq(S_.st(X), INVALID)), Imp(eq(S_.st(X), INVALID), eq(S.st(X), INVALID)))
    cl = {'running_of_phase': lambda j, a: '(fig_running_of_phase s R %s)' % ' '.join(a)}
    for rn, p in step_cases():
        out.append(case_lemma('figk', 'step_cases', RULE[rn], [X], goal, p, cl, '(s : St) (R : reachable s)'))
    out.append('End ADM.\n')
    for n in ['fig_inv_reachable'] + ['figc_' + n for n, *_ in STEP] + names:
        out.append('Print Assumptions %s.' % n)
    # the definitions and tactics of ADM.v, without its proofs
    return src[:src.index('Lemma inv_init')] + '\n'.join(out) + '\n'


def rocq_regions(src):
    out = [LTAC_REG]
    hyp = {
        'SR_Create': '(NO : NoOverlap r e Re Ra)',
        'SR_DestroyFree': '',
        'SR_Other': "(Hi : forall e, state s e = INVALID <-> state s' e = INVALID)",
    }
    cl = {
        'C1': lambda j, a: ['(proj1 (proj1 C1 %s) %s)' % (a[0], a[2]), '(proj2 (proj1 C1 %s) %s)' % (a[0], a[2]),
                            '(fun n => proj2 C1 %s %s n %s)' % (a[0], a[1], a[2])][j],
        'IH': lambda j, a: '(proj%d (IH %s))' % (j + 1, a[0]),
        'NoOverlap': lambda j, a: ['(proj1 NO %s)' % a[1], '(proj1 (proj2 NO) %s)' % a[1],
                                   '(fun n => proj2 (proj2 NO) %s n %s)' % (a[0], a[1])][j],
        'SameInvalid': lambda j, a: '(proj%d (Hi %s))' % (j + 1, a[0]),
    }
    ih = {'C1_reachable': '(C1 : C1_RegionsDisjoint r)',
          'lifecycle_reachable': '(IH : forall x, state s x = INVALID <-> (Empty (epm r x) /\\ Empty (adm r x)))'}
    goals = {'C1_reachable': C1_GOALS, 'lifecycle_reachable': LIFE_GOALS}
    for title, i, rn, p in regions():
        ev, goal = goals[title][i]
        vs = ev + ([AV] if title == 'C1_reachable' else [])
        name = 'fig_%s_%d_%s' % (title, i + 1, rn)
        if rn == 'RR_init':
            rq = Rq({}, s='init', r='(mkRg (fun _ => emptyR) (fun _ => emptyR))')
            g0 = unprime(goal)
            out.append(rq.lemma(name, '', closed(rq, vs, g0), ' '.join(rq.t(v) for v in vs), p))
            continue
        rule = next(r for r in RRULES if r.name == rn)
        post = {'SR_Create': ('(h : Hart) (e : EID) (d : Data) (m0 : Meta) (Re Ra : Region)',
                              'do_create s e d m0', 'setR r e Re Ra'),
                'SR_DestroyFree': ('(e : EID)', 'do_destroy_free s e', 'setR r e emptyR emptyR'),
                'SR_Other': ("(s' : St)", "s'", 'r')}[rn]
        rq = Rq(cl, s1='(%s)' % post[1], r1='(%s)' % post[2])
        gs = ''.join(' (G%d : %s)' % (k, rq.f(g)) for k, g in enumerate(rule.guards))
        stmt = closed(rq, vs, goal)
        pre = []
        if rn != 'SR_Other':
            pre = ["set (s' := %s)." % post[1], "set (r' := %s)." % post[2]]
            rq.s1, rq.r1 = "s'", "r'"
        else:
            rq.s1, rq.r1 = "s'", 'r'
        out.append(rq.lemma(name, '(s : St) (r : Rg) %s %s%s %s' % (post[0], ih[title], gs, hyp[rn]), stmt,
                            ' '.join(rq.t(v) for v in vs), p, pre))
    i = src.index('Section Derived.')
    src = src[:i] + '\n'.join(out) + '\n' + src[i:]
    out = []
    orig = {'SMNotShadowed': 'forall a h p, SMRegion a -> ~ MatchPerms s r h a p',
            'EPMIsolated': 'forall e h a p, epm r e a -> (MatchPerms s r h a p <-> p = EpmPerm s h e)',
            'ADMPermFaithful': 'forall e h a p, adm r e a -> (MatchPerms s r h a p <-> p = MP (pmp s e h))'}
    for (name, goal), (_, p) in zip(DERIVED, derived()):
        rq = Rq(cl)
        e = '' if name == 'SMNotShadowed' else 'e '
        out.append(rq.lemma('fig_' + name, '', '(forall %s(h : Hart) (a : Addr) (p : MPerm), %s)'
                            % (e and '(e : EID) ', rq.f(goal)), e + 'h a p', p))
        out.append('Lemma figc_%s : %s.\nProof.\n  intros %s; exact (fig_%s %sh a p).\nQed.\n'
                   % (name, orig[name], e + 'h a p' if e else 'a h p', name, e))
    i = src.index('End Derived.')
    return src[:i] + '\n'.join(out) + '\n' + src[i:]


def main_rocq(adm, regions_v, outdir):
    """Copies of ADM.v and ADMRegions.v with one lemma for each figure; the
    proof of the lemma has one tactic step for each inference of the figure."""
    open(outdir + '/FigADM.v', 'w').write(rocq_adm(open(adm).read()))
    open(outdir + '/FigRegions.v', 'w').write(rocq_regions(open(regions_v).read()))

if __name__ == '__main__':
    if sys.argv[1:2] == ['typ']:
        main_typ()
    elif sys.argv[1:2] == ['rocq']:
        main_rocq(*sys.argv[2:5])
    else:
        for name, rn, p in [r[:3] for r in preservation()] + step_properties():
            print(name, rn, '%d %s' % (p.size, ' '.join(sorted(used(p, set())))))
        for r in initial() + state_properties() + step_cases() + regions() + derived():
            print(*r[:-1], r[-1] and r[-1].size)
