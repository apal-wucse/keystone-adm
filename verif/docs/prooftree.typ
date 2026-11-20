// Proof figures (natural deduction) for PROOF.typ.
//
// A derivation is a dictionary:
//   (c: conclusion)                 a leaf (an assumption)
//   (c: conclusion, r: k)           the derivation Pi_k, shown separately
//                                   (r may also be the content to show)
//   (c: conclusion, n: rule, p: (premise, ..))
// The positions are computed here and every piece is placed in one box, so
// the depth of a derivation is not limited by the nesting of elements.

#let gap = 7pt
#let lift = 2.2pt

// -> (w, h, l, r, items): l and r bound the conclusion; an item is
// (x, y, content) from the bottom left corner
#let place-tree(t) = {
  let c = t.c
  let cs = measure(c)
  if "r" in t {
    let a = if type(t.r) == int { $Pi_#t.r$ } else { t.r }
    let d = $dots.v$
    let sa = measure(a)
    let sd = measure(d)
    let w = calc.max(cs.width, sa.width)
    return (w: w, h: cs.height + sd.height + sa.height + 2 * lift, l: 0pt, r: w, items: (
      ((w - cs.width) / 2, 0pt, c),
      ((w - sd.width) / 2, cs.height + lift, d),
      ((w - sa.width) / 2, cs.height + sd.height + 2 * lift, a),
    ))
  }
  if "n" not in t {
    return (w: cs.width, h: cs.height, l: 0pt, r: cs.width, items: ((0pt, 0pt, c),))
  }
  let lab = text(size: 0.85em, t.n)
  let ls = measure(lab)
  let subs = t.p.map(place-tree)
  let tw = subs.fold(0pt, (a, s) => a + s.w) + gap * calc.max(subs.len() - 1, 0)
  let th = subs.fold(0pt, (a, s) => calc.max(a, s.h))
  // the line spans the conclusions of the premises and the conclusion
  let l = if subs.len() == 0 { 0pt } else { subs.first().l }
  let r = if subs.len() == 0 { 0pt } else { tw - subs.last().w + subs.last().r }
  let cw = cs.width + 4pt
  let cx = (l + r - cw) / 2
  let shift = if cx < 0pt { -cx } else { 0pt }
  let l = calc.min(l, cx) + shift
  let r = calc.max(r, cx + cw) + shift
  let y1 = cs.height + lift
  let y2 = y1 + lift + 0.4pt
  let items = (
    (cx + shift + 2pt, 0pt, c),
    (l, y1, line(length: r - l, stroke: 0.4pt)),
    (r + 2pt, y1 - ls.height / 2, lab),
  )
  let x = shift
  for s in subs {
    for it in s.items {
      items.push((it.at(0) + x, it.at(1) + y2, it.at(2)))
    }
    x += s.w + gap
  }
  (
    w: calc.max(tw + shift, r + 2pt + ls.width), h: y2 + th,
    l: cx + shift + 2pt, r: cx + shift + 2pt + cs.width, items: items,
  )
}

// One figure, set inline so that small figures share a line; it is scaled
// down when it is wider than the text area.
#let tree(head: none, size: 7.5pt, t) = box(inset: (right: 16pt, y: 5pt), {
  set text(size: size)
  context {
    let pw = if page.flipped { page.height } else { page.width }
    let hw = if head == none { 0pt } else { measure(head).width + 5pt }
    let room = pw - 40mm - 16pt - hw
    let l = place-tree(t)
    let b = box(width: l.w, height: l.h, for it in l.items {
      place(bottom + left, dx: it.at(0), dy: -it.at(1), it.at(2))
    })
    let b = if l.w > room {
      let f = room / l.w * 100%
      scale(x: f, y: f, reflow: true, b)
    } else { b }
    if head == none { b } else { grid(columns: 2, column-gutter: 5pt, align: bottom, head, b) }
  }
})

// the long formula that a figure abbreviates
#let legend(body) = block(above: 0.8em, below: 0.4em, sticky: true, text(size: 7.5pt, body))
