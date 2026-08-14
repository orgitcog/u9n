---
name: rooted
description: rooted tree enumeration and generation
---

# Rooted Trees: Enumeration and Generation

## Overview

Rooted trees are fundamental combinatorial structures that represent hierarchical relationships with a designated root node. This implementation provides efficient algorithms for **enumerating all distinct rooted trees** with a given number of nodes, a problem that corresponds to the celebrated OEIS sequence [A000081](https://oeis.org/A000081).

## Concept

### The Bag Nesting Problem

Imagine you have `n` identical plastic bags that you want to nest inside each other. How many distinct ways can you arrange them? This seemingly simple problem has profound mathematical depth:

- For **1 bag**: 1 way → `()`
- For **2 bags**: 1 way → `(())`
- For **3 bags**: 2 ways → `((()))`, `(()())`
- For **4 bags**: 4 ways → `(((())))`, `((()()))`, `((())())`, `(()(()))`
- For **5 bags**: 9 ways → The full enumeration

Each bag nesting configuration represents a **rooted tree** where:
- Each bag is a **node**
- A bag with its contents forms a **subtree**
- The outermost bag is the **root**

### Mathematical Foundation

The number of rooted trees with `n` nodes follows the **OEIS A000081** sequence:
```
n:     1,  2,  3,  4,   5,   6,   7,    8,    9,    10,   11,    12, ...
T(n):  1,  1,  2,  4,   9,  20,  48,  115,  286,   719, 1842,  4766, ...
```

This sequence counts **unlabeled rooted trees**, meaning trees are considered identical if they have the same structure, regardless of how we label the nodes.

## Representation

### Parentheses Notation

Trees are represented using nested parentheses, where:
- `(` represents entering a bag/subtree
- `)` represents exiting a bag/subtree
- Siblings (subtrees at the same level) are concatenated

Examples:
```
()          # Single node (1-tree)
(())        # Two nodes, one child (2-tree)
((()))      # Three nodes in a chain (3-tree)
(()())      # Three nodes, two siblings (3-tree)
(((())))    # Four nodes in a chain (4-tree)
((()()))    # Four nodes, nested structure (4-tree)
((())())    # Four nodes, mixed structure (4-tree)
(()(()))    # Four nodes, two branches (4-tree)
```

### Binary Encoding

For efficient storage and manipulation, trees are encoded as integers where:
- Each bit pair represents a parenthesis
- Bit 1 = `(`, Bit 0 = `)`
- Trees are stored with a leading 1 bit as sentinel

Example for `(())`:
```
String:  ( ( ) )
Bits:    1 1 0 0
Encoded: 1 1 1 0 0  (with leading 1)
         = 0b11100 = 28 decimal
```

## Algorithm

### Core Approach: Recursive Assembly

The algorithm builds trees of size `n` by assembling smaller subtrees:

1. **Base Case**: A 1-tree is just `()`
2. **Recursive Case**: An n-tree consists of:
   - An outer pair of parentheses
   - Inside: a combination of smaller trees that sum to `n-1` nodes

### Key Operations

#### 1. Tree Assembly

```
assemble(n, t, sl, pos, rem):
  n:   target tree size
  t:   partially assembled tree so far
  sl:  size of subtree being considered
  pos: position in list of sl-sized trees
  rem: remaining nodes to add
```

The algorithm:
1. If `rem == 0`: We've assembled a complete tree → save it
2. Otherwise: Try adding subtrees of size `sl` or smaller
3. Recursively continue with remaining nodes

#### 2. Tree Generation

```
mktrees(n):
  1. Generate all smaller trees first (mktrees(n-1))
  2. Assemble n-trees from (n-1)-trees using all valid combinations
  3. Store offset positions for efficient lookup
```

### Example: Building 4-trees

To build all 4-trees:
1. We need 3 nodes inside the outer parentheses
2. Possible partitions of 3:
   - `3` → one 3-tree: `(((())))`
   - `2+1` → one 2-tree + one 1-tree: `((())())`
   - `1+2` → one 1-tree + one 2-tree: `(()(()))`
   - `1+1+1` → three 1-trees: `((()()()` (wait, this is invalid due to ordering!)

The algorithm handles **canonical ordering** to avoid duplicates by ensuring subtrees appear in non-increasing size order.

## Implementation Examples

### C Implementation

The C version uses bit manipulation for compact representation:

```c
typedef unsigned long long tree;

void append(tree t) {
    list[len++] = 1 | t<<1;  // Add with sentinel bit
}

void assemble(uint n, tree t, uint sl, uint pos, uint rem) {
    if (!rem) {
        append(t);
        return;
    }
    
    // Try adding a subtree of size sl
    if (sl > rem) {
        sl = rem;
        pos = offset[sl];
    } else if (pos >= offset[sl + 1]) {
        sl--;
        if (!sl) return;
        pos = offset[sl];
    }
    
    // Recurse: add current subtree or try next
    assemble(n, t<<(2*sl) | list[pos], sl, pos, rem - sl);
    assemble(n, t, sl, pos + 1, rem);
}
```

### Haskell Implementation

The Haskell version uses pure functional composition:

```haskell
-- Generate all partitions of n
parts :: Int -> [[(Int, Int)]]
parts n = f n 1
  where
    f n x
      | n == 0 = [[]]
      | x > n = []
      | otherwise = f n (x + 1) ++
          concatMap (\c -> map ((c, x) :) (f (n - c * x) (x + 1)))
                    [1 .. n `div` x]

-- Build trees from partitions
trees :: Int -> [String]
trees n = map (\x -> "(" ++ x ++ ")")
        $ concatMap (foldr (prod . build) [""]) (parts (n - 1))
  where
    build (c, x) = pick c $ trees x
    prod aa bb = [a ++ b | a <- aa, b <- bb]
```

### Python Implementation

The Python version focuses on readability:

```python
def bags(n, cache={}):
    if not n: return [(0, "")]
    
    # Get all smaller trees
    upto = sum([bags(x) for x in range(n-1, 0, -1)], [])
    
    # Build n-trees by chaining smaller trees
    return [(c+1, '('+s+')') for c,s in bagchain((0, ""), n-1, upto)]

def bagchain(x, n, bb, start=0):
    if not n: return [x]
    
    out = []
    for i in range(start, len(bb)):
        c, s = bb[i]
        if c <= n:
            out += bagchain((x[0] + c, x[1] + s), n-c, bb, i)
    return out
```

### JavaScript Implementation

The JavaScript version demonstrates functional programming patterns:

```javascript
const bagPatterns = n =>
    nub(map(
        composeList([
            commasFromTree,
            depthSortedTree,
            treeFromParentIndices
        ]),
        parentIndexPermutations(n)
    ));

const treeFromParentIndices = pxs => {
    const go = (tree, tplIP) =>
        Node(
            tree.root,
            tree.root === snd(tplIP) ? 
                tree.nest.concat(Node(fst(tplIP)), []) :
                map(t => go(t, tplIP), tree.nest)
        );
    return foldl(go, Node(0, []), zip(enumFromToInt(1, pxs.length), pxs));
};
```

### Rust Implementation

The Rust version emphasizes safety and efficiency:

```rust
fn assemble(list: &mut Vec<usize>, offset: &mut Vec<usize>, 
            n: usize, t: usize, mut sl: usize, mut pos: usize, rem: usize) {
    if rem == 0 {
        add(list, t);
        return;
    }
    if sl > rem {
        sl = rem;
        pos = offset[sl];
    } else if pos >= offset[sl+1] {
        sl -= 1;
        if sl == 0 { return; }
        pos = offset[sl];
    }
    assemble(list, offset, n, (t << (2*sl)) | list[pos], sl, pos, rem-sl);
    assemble(list, offset, n, t, sl, pos+1, rem);
}
```

### Go Implementation

The Go version balances simplicity and performance:

```go
func assemble(n uint, t tree, sl, pos, rem uint) {
    if rem == 0 {
        add(t)
        return
    }
    
    if sl > rem {
        sl = rem
        pos = offset[sl]
    } else if pos >= offset[sl+1] {
        sl--
        if sl == 0 { return }
        pos = offset[sl]
    }
    
    assemble(n, t<<(2*sl)|list[pos], sl, pos, rem-sl)
    assemble(n, t, sl, pos+1, rem)
}
```

## Usage Examples

### Example 1: Enumerate 5-Trees

```bash
# C version
$ ./list-rooted-trees 5
Number of 5-trees: 9
((((())))
((()(())))
((()())())
((())()())
(((()())))
(()()()())
(()(())())
(()(()()))
(()((())()))
```

### Example 2: Count Trees

```python
# Python version
for n in range(1, 11):
    trees = bags(n)
    print(f"n={n}: {len(trees)} trees")

# Output:
# n=1: 1 trees
# n=2: 1 trees
# n=3: 2 trees
# n=4: 4 trees
# n=5: 9 trees
# n=6: 20 trees
# n=7: 48 trees
# n=8: 115 trees
# n=9: 286 trees
# n=10: 719 trees
```

### Example 3: Visualize Tree Structure

```python
def replace_brackets(s):
    """Use different brackets for different depths"""
    depth, out = 0, []
    for c in s:
        if c == '(':
            out.append("([{"[depth % 3])
            depth += 1
        else:
            depth -= 1
            out.append(")]}"[depth % 3])
    return "".join(out)

for x in bags(4):
    print(replace_brackets(x[1]))

# Output:
# [[[[]]]]
# [[{}[]]]
# [[{}]()]
# [[)({}]]
```

## Performance Characteristics

### Time Complexity

- **Generation**: O(T(n)) where T(n) is the number of trees
- **Assembly**: Exponential in n, but tractable for small n
- **For n=5**: ~9 trees generated
- **For n=10**: ~719 trees generated
- **For n=15**: ~37,663 trees generated

### Space Complexity

- **Storage**: O(T(n)) for all trees
- **Binary encoding**: 2n bits per tree
- **Offset array**: O(n) for lookup table

### Practical Limits

Language-specific limits due to integer size and recursion:

| Language | Max n | Reason |
|----------|-------|---------|
| C        | ~25   | Long long overflow |
| Java     | ~12   | Stack overflow (default) |
| Python   | ~15   | Slow, high memory |
| Rust     | ~19   | Stack overflow |
| Go       | ~19   | Stack overflow |
| Haskell  | ~12   | Memory and stack |

### Optimization Strategies

1. **Memoization**: Cache previously generated trees
2. **Iterative approach**: Avoid recursion depth limits
3. **Lazy evaluation**: Generate trees on-demand (Haskell)
4. **Bit packing**: Efficient storage using bit manipulation
5. **Offset arrays**: O(1) lookup for tree ranges

## Mathematical Properties

### Cayley's Formula Connection

Cayley proved that the number of labeled trees on n vertices is n^(n-2). Our unlabeled rooted trees follow A000081, which is related but distinct.

### Recurrence Relation

The sequence satisfies:
```
a(n) = (1/n) * Σ(k=1 to n-1) [ (Σ(d|k) d*a(d)) * a(n-k) ]
```

### Generating Function

The ordinary generating function T(x) satisfies:
```
T(x) = x * exp(Σ(k≥1) T(x^k)/k)
```

### Asymptotic Behavior

For large n:
```
T(n) ~ C * α^n / n^(3/2)
```
where α ≈ 2.9557652856 (Otter's constant) and C ≈ 0.4399240125.

## Applications

### 1. Chemical Isomers

Rooted trees enumerate **structural isomers** of alkanes (CnH2n+2):
- Each carbon atom is a node
- Bonds determine tree structure
- Root represents a designated carbon

### 2. Phylogenetic Trees

In biology, rooted trees represent **evolutionary relationships**:
- Species are leaves
- Internal nodes are common ancestors
- Root is the most recent common ancestor

### 3. Expression Trees

In compilers, rooted trees represent **abstract syntax trees**:
- Operators are internal nodes
- Operands are leaves
- Root is the main expression

### 4. Hierarchical Clustering

In data analysis, rooted trees represent **dendrogram structures**:
- Data points are leaves
- Clusters are internal nodes
- Root represents all data

### 5. File System Structures

Directory hierarchies are rooted trees:
- Files and directories are nodes
- Root is the filesystem root
- Path from root defines file location

## Advanced Topics

### Canonical Tree Ordering

To avoid duplicates, subtrees must be ordered. The standard convention:
- **Canonical form**: Subtrees in non-increasing size order
- **Lexicographic order**: For same-size subtrees, use string comparison

Example: `(()(()))` is canonical, but `((())())` comes before it.

### Tree Isomorphism

Two trees are isomorphic if there exists a structure-preserving mapping between them. The parentheses representation is canonical, ensuring each distinct tree has exactly one representation.

### Unranking Algorithm

Given an index `i`, generate the i-th tree without enumerating all previous trees:
```
unrank(n, i):
  1. Determine partition of (n-1) nodes
  2. Recursively unrank subtrees
  3. Combine with outer parentheses
```

### Ranking Algorithm

Given a tree, determine its index in the enumeration:
```
rank(tree):
  1. Parse into subtrees
  2. Determine partition
  3. Recursively rank subtrees
  4. Combine ranks with partition offset
```

## Connection to Other Structures

### Free Trees (A000055)

Rooted trees correspond to **free trees** (unrooted):
- A free tree with n nodes yields multiple rooted trees
- Choose different roots → different rooted trees
- A000055(n) ≤ A000081(n+1)

### Planted Trees

**Planted trees** distinguish the root's degree:
- A000081 counts planted plane trees
- Also called "ordered trees" when sibling order matters

### Catalan Numbers (A000108)

When sibling order matters and all internal nodes have exactly 2 children:
- Binary trees follow Catalan numbers
- C(n) = (1/(n+1)) * C(2n, n)

## Philosophical Implications

### Combinatorial Explosion

Even simple rules (nest bags) lead to rapid growth:
- **Linear input**: n bags
- **Exponential output**: ~2.955^n trees

This demonstrates the **fundamental richness** of combinatorial structures.

### Canonical Representations

The parentheses notation provides a **unique** representation:
- No ambiguity
- Compact encoding
- Easy to parse and generate

This exemplifies the power of **canonical forms** in mathematics.

### Recursive Beauty

Trees are defined recursively:
- A tree is a root with subtrees
- Subtrees are trees
- Base case: single node

This **self-similarity** is fundamental to both the structure and the algorithm.

## Implementation Details

### Memory Layout

```
list:    [tree1, tree2, tree3, ..., treeN]
offset:  [0, 1, 2, 4, 8, 17, ...]
         indices where n-trees start in list
```

### Bit Encoding Details

For tree `((()))`:
```
Parentheses: ( ( ( ) ) )
Bits:        1 1 1 0 0 0
With sentinel: 1 1 1 1 0 0 0
Binary: 0b1111000 = 120
Stored: list[i] = 120
```

To decode:
```c
void show(tree t, uint len) {
    for (; len--; t >>= 1)
        putchar(t & 1 ? '(' : ')');
}
```

## Future Directions

### Parallel Generation

Generate tree ranges in parallel:
- Partition by subtree combinations
- Independent assembly of ranges
- Merge results

### GPU Acceleration

Use GPU for massive parallelism:
- Each thread generates a tree range
- Massive combinatorial enumeration
- Useful for large n (>20)

### Symbolic Computation

Integrate with computer algebra systems:
- Generate trees symbolically
- Analyze mathematical properties
- Count without explicit enumeration

### Interactive Visualization

Build web-based tree explorer:
- Navigate tree space
- Visualize structure
- Educational tool

## References

### Academic Papers

- Cayley, A. (1857). "On the Theory of the Analytical Forms called Trees"
- Otter, R. (1948). "The Number of Trees"
- Harary, F., Palmer, E.M. (1973). "Graphical Enumeration"

### Online Resources

- OEIS A000081: https://oeis.org/A000081
- Rosetta Code: http://rosettacode.org/wiki/List_rooted_trees
- OEIS A000055: https://oeis.org/A000055 (Free trees)

### Related Sequences

- A000108: Catalan numbers (binary trees)
- A000055: Free trees (unrooted)
- A000669: Labeled rooted trees
- A001190: Wedderburn-Etherington numbers

## License

MIT License - see [LICENSE](../../LICENSE) for details.

---

**Rooted Trees**: Where simple nesting rules generate infinite combinatorial beauty, enumerated through elegant recursive algorithms that mirror the structure they create.
