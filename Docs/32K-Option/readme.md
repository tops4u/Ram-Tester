# Testing 32K RAM: 4164 / 3732 / 4532

## The Simple Version

A **4164** is a 64K×1 DRAM. When it has defects, it may still be usable as a **32K×1** chip — either as an **OKI MSM3732** or a **TI TMS4532**. This tester figures that out automatically.

### What happens when you test a chip?

- **4164**: If no errors are found → reported as `4164 64K x 1`. If errors are found but a good 32K half exists → the tester reports which 3732 or 4532 type it could still serve as. Only if there is no usable 32K block will it show as defective.

- **3732 or 4532**: The tester checks all four quadrants of the chip. If only one quadrant has errors, the chip may work not only as the type printed on it, but also as the *other* type (e.g. a 3732-H could also work as a 4532-4). The tester reports all valid options.

- **If errors span two adjacent quadrants**: The chip can only work as the specific subtype printed on it (e.g. `3732-L 32K`), if that half is error-free.

### Important

- Make sure your firmware has the 32K option enabled. Check the [Software section](../../Software) for instructions.
- When 32K support is enabled, pay attention to 4164 results — a chip with errors may still show as a valid 3732 or 4532 rather than simply "defective".
- If you never test 3732 or 4532 and find the extra output confusing, you can install the firmware with 32K logic disabled.

---

## The Technical Details

### Why do 32K chips exist?

In the early 1980s, 4164 RAM was in high demand but production yields were imperfect. Rather than discarding chips with partial defects, two manufacturers found a way to sell them as 32K×1 chips:

- **OKI** sold them as the **MSM3732**
- **Texas Instruments** sold them as the **TMS4532**

Both split the 4164's 256×256 matrix in half using address line **A7**, reducing the addressable space from 256 to 128 on one axis. But they chose different axes:

| Manufacturer | Chip | Split Axis | Variants |
|---|---|---|---|
| OKI | MSM3732 | **Columns** (left/right) | **-L** (cols 0–127) · **-H** (cols 128–255) |
| TI | TMS4532 | **Rows** (upper/lower) | **-3** aka NL3 (rows 0–127) · **-4** aka NL4 (rows 128–255) |

The ZX Spectrum is the most well-known computer to use these chips for its upper RAM. The board had jumpers to select the type and subtype — you had to populate all sockets with the same variant.

### The Quadrant Approach

This tester doesn't just check one half — it evaluates the entire 4164 matrix and splits it into **four quadrants** at A7 on both axes:

```
         Col 0–127       Col 128–255
        (Col A7 = 0)    (Col A7 = 1)
       ┌──────────────┬──────────────┐
Row    │              │              │
0–127  │     Q1       │     Q2       │
(A7=0) │              │              │
       ├──────────────┼──────────────┤
Row    │              │              │
128–255│     Q3       │     Q4       │
(A7=1) │              │              │
       └──────────────┴──────────────┘
```

Each quadrant holds 16K bits (128×128). A valid 32K chip needs **two adjacent quadrants** that are error-free:

| Good Quadrants | Usable As | Split |
|---|---|---|
| Q1 + Q2 | **TMS4532-3** | Upper rows (Row A7 = 0) |
| Q3 + Q4 | **TMS4532-4** | Lower rows (Row A7 = 1) |
| Q1 + Q3 | **MSM3732-L** | Left columns (Col A7 = 0) |
| Q2 + Q4 | **MSM3732-H** | Right columns (Col A7 = 1) |

**Diagonal combinations** (Q1+Q4 or Q2+Q3) are **not valid** — they don't form a contiguous address block because A7 cannot be 0 and 1 at the same time.

### Cross-Type Compatibility

Because the 4532 and 3732 split along *different axes*, a chip can sometimes serve as either type:

**Example:** A chip has errors only in Q3 (lower-left). Three quadrants are good (Q1, Q2, Q4).
- ✅ Works as **TMS4532-3** (Q1+Q2, upper rows — no errors)
- ✅ Works as **MSM3732-H** (Q2+Q4, right columns — no errors)
- ❌ Fails as **TMS4532-4** (Q3+Q4 — Q3 has errors)
- ❌ Fails as **MSM3732-L** (Q1+Q3 — Q3 has errors)

The tester reports **all** valid configurations, not just the type printed on the chip. As far as we know, no other tester does this kind of cross-type evaluation.

For a visual explanation with diagrams, see the [Quadrant Documentation (EN)](4164_Quadrants_Explanation.pdf) or [Quadranten-Dokumentation (DE)](4164_Quadranten_Erklärung.pdf).
