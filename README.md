# 📜 Gram Text Editor

**Gram** is a lightweight, terminal-based text editor written in C, inspired by [Kilo](https://viewsourcecode.org/snaptoken/kilo/). It provides essential text editing functionalities while maintaining a minimalistic and efficient codebase.

---

## 🚀 Features

- Basic text editing operations.
- Handles input, output, and cursor movements.
- **Relative line numbering** for better navigation.
- **Jump up/down by N lines** using shortcut keys.
- Syntax highlighting (optional).
- Small codebase (~1000 lines of C).
- Runs in a terminal (Linux/macOS support).
- No external dependencies.

---

## 🛠 Installation

Clone the repository and compile the source code using `gcc`:

```sh
git clone https://github.com/yourusername/gram-text-editor.git
cd gram-text-editor
gcc gram.c -o gram -Wall -Wextra -pedantic -std=c99
```

Then, run it:

```sh
./gram filename.txt
```

---

## 📖 Usage

Once inside **Gram**, you can use the following keybindings:

| Command       | Description                           |
|--------------|---------------------------------------|
| `Ctrl + S`   | Save file                            |
| `Ctrl + Q`   | Quit the editor                      |
| `Ctrl + F`   | Find text in the file                |
| `Arrow Keys` | Move cursor                          |
| `Backspace`  | Delete character before cursor       |
| `Enter`      | Insert new line                      |
| `Ctrl + U`   | **Jump up N lines** (configurable)   |
| `Ctrl + D`   | **Jump down N lines** (configurable) |

---

## 🏗 Project Structure

```
📂 gram-text-editor
 ├── gram.c         # Main source file
 ├── gram.h         # Header file
 ├── Makefile       # Build system
 ├── README.md      # Documentation
```

---

## ⚙️ How It Works

### 1️⃣ Raw Mode
The editor disables canonical mode using `tcgetattr()` and `tcsetattr()`, allowing real-time keypress reading.

### 2️⃣ Input Handling
Gram reads user input character-by-character and processes commands accordingly.

### 3️⃣ Cursor Movement & Relative Numbering
- The editor tracks cursor position and displays **relative line numbers**.
- Implements **jumping** functionality (`Ctrl + U` / `Ctrl + D`) to move N lines at once.

### 4️⃣ File I/O
- It loads a file into memory when opened.
- Saves modifications back to disk when needed.

---

## 🛠️ Build & Run with Makefile

Instead of manually compiling, you can use a `Makefile`:

```make
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

gram: gram.c
    $(CC) $(CFLAGS) gram.c -o gram

clean:
    rm -f gram
```

Build and run:

```sh
make
./gram filename.txt
```

---

## 🏗 Roadmap & Future Improvements

- [x] **Relative numbering**
- [x] **Jump N lines up/down**
- [ ] Add syntax highlighting.
- [ ] Implement undo/redo functionality.
- [ ] Line numbering toggle support.
- [ ] Mouse support for selection.
- [ ] Windows support.



## 🤝 Contributing

Pull requests and suggestions are welcome! Feel free to fork the repo and submit improvements.

---

## 📬 Contact

📧 **shivamaher100@gmail.com**
