// Style of MODEL.typ and MODEL.en.typ.  IPAex fonts come from --font-path.
#let serif = ("Libertinus Serif", "IPAexMincho")
#let sans = ("Libertinus Serif", "IPAexGothic")

#let doc(title: none, lang: "en", body) = {
  set document(title: title)
  set page(paper: "a4", margin: (x: 20mm, y: 22mm), numbering: "1")
  set text(font: serif, size: 10pt, lang: lang)
  set par(leading: 0.75em, spacing: 1.1em)
  show strong: set text(font: sans)
  show heading: set text(font: sans)
  show heading: set block(above: 1.6em, below: 0.9em)
  show heading.where(level: 1): set text(size: 14pt)
  show heading.where(level: 2): set text(size: 12pt)
  show heading.where(level: 3): set text(size: 10.5pt)
  show raw: set text(font: ("DejaVu Sans Mono", "IPAexGothic"), size: 0.9em)
  show raw.where(block: true): block.with(
    width: 100%, inset: 8pt, radius: 2pt, fill: luma(245))
  set table(stroke: 0.5pt + luma(160), inset: 5pt)
  show table: set text(size: 9pt)
  show table: set par(leading: 0.6em)
  set list(indent: 0.5em)
  set enum(indent: 0.5em)

  align(center, text(font: sans, size: 18pt, weight: "bold", title))
  v(1em)
  outline(depth: 2)
  pagebreak()
  body
}
