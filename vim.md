In `vim`, there are two very fast ways to delete everything in the file.

Here is the quickest method:

1. Press **`Esc`** (to make sure you are in Normal mode).
2. Type **`gg`** (this jumps your cursor to the very first line).
3. Type **`dG`** (this deletes everything from your current line to the bottom of the file).

**Alternative method (using a command):**

1. Press **`Esc`**.
2. Type **`:%d`** and hit **`Enter`**.

Once the file is empty, you can press **`i`** to go into Insert mode and paste the new code!

You are exactly right—it is basically cut (or copy) and paste! Vim just uses its own terminology: **`d`** for delete (which actually cuts), **`y`** for yank (copy), and **`p`** for put (paste).

Here are the two easiest ways to do it.

### Method 1: The Fast "Vim Way" (Number + Command)

If you know exactly how many lines you want to move, this is blazing fast:

1. Press **`Esc`** to ensure you are in Normal mode.
2. Put your cursor anywhere on the first line you want to move.
3. Type **`3dd`** to cut 3 lines (or **`3yy`** if you just want to copy them). The lines will disappear.
4. Move your cursor to where you want the lines to go.
5. Press **`p`** to paste them on the line *below* your cursor (or **`Shift + p`** to paste *above*).

### Method 2: The Visual Way (Highlighting)

If you prefer to highlight the text first so you can see exactly what you are grabbing:

1. Press **`Esc`**.
2. Put your cursor on the first line.
3. Press **`Shift + v`** to enter "Visual Line" mode.
4. Press the **Down Arrow** (or **`j`**) twice to highlight your three lines.
5. Press **`d`** to cut them (or **`y`** to copy).
6. Move to your destination and press **`p`** to paste.