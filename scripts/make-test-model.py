#!/usr/bin/env python3
"""Deterministic tiny llama2.c checkpoint + tokenizer for testing the local-LLM engine.

The weights are pseudo-random (fixed seed), so the text is meaningless; the point is that
the kernel engine and llama2.c's reference run.c produce exactly the same tokens.
Format: llama2.c legacy float32 checkpoint (7 x int32 header + weights) and tokenizer.bin.
Usage: make-test-model.py <out_dir>"""
import random, struct, sys
from pathlib import Path

out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
dim, hidden, layers, heads, kv_heads, seq_len = 64, 176, 2, 4, 2, 128

# Vocabulary: <unk> <s> </s>, 256 byte tokens, then merges (unique strings, descending scores).
merges = [' ', 'a', 'e', 'o', 'i', 'u', 's', 'r', 'n', 't', 'd', 'm', 'P', 'g', ' a', ' e', ' o', ' d', ' P',
          'er', 're', 'in', 'us', 'gr', 'Pe', 'eg', 'ri', 'nu', 'de', 'os', 'as', 'es', 'ra', 'ar', 'or', ' de',
          ' Pe', 'Per', 'Pere', 'egr', 'rin', 'inus', 'grinus', 'Peregrinus', ' Peregrinus', 'Ola', ' mundo',
          'mundo', 'sistema', ' sistema', 'kernel', ' kernel', 'micro', ' micro', '.', ',', '!', '?', '\n']
vocab = [('<unk>', 0.0), ('<s>', 0.0), ('</s>', 0.0)] + [('<0x%02X>' % b, 0.0) for b in range(256)]
vocab += [(m, -float(i)) for i, m in enumerate(merges)]
assert len({v for v, _ in vocab}) == len(vocab)
vocab_size = len(vocab)

with open(out / 'tokenizer.bin', 'wb') as f:
    pieces = [v.encode('utf-8') for v, _ in vocab]
    f.write(struct.pack('<i', max(len(p) for p in pieces)))
    for (v, score), p in zip(vocab, pieces):
        f.write(struct.pack('<fi', score, len(p)) + p)

rng = random.Random(20261004)
head_size = dim // heads
kv_dim = dim * kv_heads // heads
counts = [vocab_size * dim, layers * dim, layers * dim * dim, layers * dim * kv_dim, layers * dim * kv_dim,
          layers * dim * dim, layers * dim, layers * dim * hidden, layers * hidden * dim, layers * dim * hidden,
          dim, seq_len * head_size]
names = ['emb', 'rms_att', 'wq', 'wk', 'wv', 'wo', 'rms_ffn', 'w1', 'w2', 'w3', 'rms_final', 'freq_cis']
with open(out / 'model.bin', 'wb') as f:
    f.write(struct.pack('<7i', dim, hidden, layers, heads, kv_heads, vocab_size, seq_len))  # positive vocab: shared classifier
    for name, n in zip(names, counts):
        if name.startswith('rms'):
            vals = [1.0 + rng.uniform(-0.1, 0.1) for _ in range(n)]
        elif name == 'freq_cis':
            vals = [0.0] * n
        else:
            vals = [rng.uniform(-0.25, 0.25) for _ in range(n)]
        f.write(struct.pack('<%df' % n, *vals))
print(out / 'model.bin', out / 'tokenizer.bin', 'vocab', vocab_size)
