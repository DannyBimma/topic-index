# topic-index

`topic-index` is a tiny yet capable **ANSI-C (C89)** command-line program that analyses plain-text to answer one question:

> _How much of this text is really about the thing I care about?_

It does so by computing an _index_ of the supplied **topic word** and reporting
statistics for the topic plus the four next most-frequent (non-stop) words in
the document.

The program is single-pass, memory-safe, and makes no assumptions about the
size of the input—everything is streamed.

---

## Features

- Calculates, for each reported word:
  - total occurrences
  - percentage of all words
  - number/percentage of sentences containing the word
- Ignores capitalisation (case-insensitive parsing).
- Skips common English stop-words for the _"other 4"_ most-used words so you
  don't waste time reading about _the_, _and_, _to_ …
- Handles arbitrarily large text—reads a byte at a time and grows buffers
  safely.
- Works with **stdin** _or_ a file path; this lets you process PDFs, Word docs,
  or anything else by piping a text extractor:
  ```sh
  pdftotext report.pdf - | ./topic_index cars
  ```
- Zero dynamic-memory leaks (checked with `valgrind`).

---

## Building

Any C89-compliant compiler will do. Typical on macOS or Linux:

```sh
cc -std=c89 -Wall -Wextra -pedantic topic-index/topic_index.c -o topic_index
```

If you prefer `gcc`/`clang`, substitute accordingly. No third-party
libraries are required.

---

## Usage

```
./topic_index <topic_word> [file]
```

- **`<topic_word>`** Word you want to measure (case-insensitive).
- **`file`** Optional plain-text file. If omitted, the program reads from
  **stdin**.

### Examples

1. Analyse a plain-text file directly:

   ```sh
   ./topic_index climate climate_report.txt
   ```

2. Analyse whatever you type (press _Ctrl-D_ / _Ctrl-Z_ when done):

   ```sh
   ./topic_index pizza
   ```

3. Analyse a PDF via `pdftotext` (uses stdin):
   ```sh
   pdftotext kitchen_appliances.pdf - | ./topic_index microwave
   ```

Sample output:

```
=============================
Topic index report
Topic word: 'cars'
Total words: 1842
Total sentences: 97
=============================
Word                 Count   % Words        Sentences   % Sent
-------------------------------------------------------------------
cars                 123      6.68%      52/97         53.61%
vehicle              57       3.09%      31/97         31.96%
engine               44       2.39%      29/97         29.90%
road                 41       2.22%      27/97         27.84%
fuel                 38       2.06%      24/97         24.74%
=============================
```

---

## How It Works (Technical Overview)

1. **Streaming Lexer** – Bytes are read via `fgetc`. Alphanumeric sequences
   form words; punctuation that ends in `.` `!` or `?` triggers a sentence
   boundary.
2. **Normalisation** – Each word is lower-cased so _"Car"_ and _"cars"_ map to
   `cars`.
3. **Hash Table** – A 10 007-bucket open-chained table keeps one `WordEntry`
   per unique word, storing:
   - `count` – total occurrences.
   - `sentence_count` – how many distinct sentences contain the word.
4. **Stop-Words** – Before selecting the _other_ 4 most-used words the program
   skips items found in a small built-in stop-word array (extendable in source).
5. **Sorting** – After the stream is consumed, an array of pointers to every
   `WordEntry` is `qsort`-ed by frequency so the top entries are found quickly.
6. **Percentages** – Simple division against total counts provides the report
   metrics.
7. **Memory Management** – All allocations go through a checked `xmalloc`
   (and friends); everything is `free`d before exit.

---

## Customisation

- **Stop-word list** – Open `topic_index.c`, locate `STOP_WORDS`, and append or
  remove words as needed.
- **Hash size / performance** – `HASH_SIZE` is prime; bump it if you expect
  millions of unique words.
- **Sentence rules** – By default `.` `!` `?` mark an end. Adjust the test in
  the main loop if you need finer control.

---

## Limitations

- Only basic ASCII letters/digits are considered part of words. UTF-8 works
  if bytes happen to be ASCII; otherwise extend `isalnum` checks.
- Non-English stop-words aren't included; add your own.
- Sentence detection is naïve (e.g. "Dr." counts as an end).

---

## License

Distributed under the terms of the [MIT License](LICENSE).
