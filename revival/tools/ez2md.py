#!/usr/bin/env python3
"""Convert ATK/ez format documents to Markdown."""

import re
import sys
import argparse
from dataclasses import dataclass, field


@dataclass
class EmbeddedObject:
    obj_type: str
    obj_id: str
    content: str


@dataclass
class ParseState:
    pos: int = 0
    footnotes: list = field(default_factory=list)
    active_markers: set = field(default_factory=set)


def parse_ez(text):
    state = ParseState()
    outer_type, outer_id, body = strip_outer_envelope(text)
    if body is None:
        return text
    md = convert_body(body, state)
    md = collapse_blank_lines(md)
    if state.footnotes:
        md += "\n\n---\n\n"
        for i, note in enumerate(state.footnotes, 1):
            md += f"[^{i}]: {note}\n"
    return md


def strip_outer_envelope(text):
    m = re.match(r'\\begindata\{(\w+),(\d+)\}\s*\n', text)
    if not m:
        return None, None, None
    obj_type, obj_id = m.group(1), m.group(2)
    body = text[m.end():]
    end_pat = re.compile(r'\\enddata\{' + re.escape(obj_type) + r',\s*' + re.escape(obj_id) + r'\}')
    end_matches = list(end_pat.finditer(body))
    if end_matches:
        body = body[:end_matches[-1].start()]
    return obj_type, obj_id, body


def convert_body(text, state):
    text = strip_header_lines(text)
    text = strip_style_definitions(text)
    text = process_embedded_objects(text, state)
    text = convert_formatting(text, state)
    text = clean_continuation_backslashes(text)
    return text


def strip_header_lines(text):
    text = re.sub(r'^\\textdsversion\{\d+\}\s*\n', '', text)
    text = re.sub(r'^\\template\{[^}]*\}\s*\n', '', text)
    return text


def strip_style_definitions(text):
    result = []
    i = 0
    while i < len(text):
        m = re.match(r'\\define\{', text[i:])
        if m:
            depth = 0
            j = i + m.start()
            while j < len(text):
                if text[j] == '{':
                    depth += 1
                elif text[j] == '}':
                    depth -= 1
                    if depth == 0:
                        j += 1
                        while j < len(text) and text[j] in ' \t':
                            j += 1
                        if j < len(text) and text[j] == '\n':
                            j += 1
                        break
                j += 1
            i = j
        else:
            result.append(text[i])
            i += 1
    return ''.join(result)


def process_embedded_objects(text, state):
    result = []
    i = 0
    while i < len(text):
        m = re.match(r'\\begindata\{(\w+),(\d+)\}\s*\n', text[i:])
        if m:
            obj_type, obj_id = m.group(1), m.group(2)
            content_start = i + m.end()
            end_pat = re.compile(r'\\enddata\{' + re.escape(obj_type) + r',\s*' + re.escape(obj_id) + r'\}')
            end_m = end_pat.search(text, content_start)
            end_idx = end_m.start() if end_m else -1
            if end_idx < 0:
                result.append(text[i])
                i += 1
                continue
            obj_content = text[content_start:end_idx]
            after_end = end_m.end()
            # skip trailing newline after enddata
            if after_end < len(text) and text[after_end] == '\n':
                after_end += 1
            # consume the \view{...} line if present
            view_m = re.match(r'\\view\{\w+,\d+,[^}]*\}', text[after_end:])
            if view_m:
                after_end += view_m.end()
            rendered = render_embedded(obj_type, obj_id, obj_content, state)
            result.append(rendered)
            i = after_end
        else:
            result.append(text[i])
            i += 1
    return ''.join(result)


def render_embedded(obj_type, obj_id, content, state):
    if obj_type == 'bp':
        return '\n---\n\n'
    elif obj_type == 'text':
        inner_state = ParseState()
        inner_state.footnotes = state.footnotes
        return convert_body(content, inner_state)
    elif obj_type == 'fnote':
        note_text = content.strip()
        note_text = strip_header_lines(note_text)
        note_text = clean_continuation_backslashes(note_text).strip()
        state.footnotes.append(note_text)
        return f'[^{len(state.footnotes)}]'
    elif obj_type == 'raster':
        return f'\n\n<!-- [embedded raster image: {obj_id}] -->\n\n'
    elif obj_type == 'table':
        return f'\n\n<!-- [embedded table: {obj_id}] -->\n\n'
    elif obj_type == 'eq':
        return f'\n\n<!-- [embedded equation: {obj_id}] -->\n\n'
    elif obj_type == 'figure':
        return f'\n\n<!-- [embedded figure: {obj_id}] -->\n\n'
    elif obj_type == 'fad':
        return f'\n\n<!-- [embedded animation: {obj_id}] -->\n\n'
    elif obj_type == 'image':
        return f'\n\n<!-- [embedded image: {obj_id}] -->\n\n'
    elif obj_type == 'link':
        return f'<!-- [link: {obj_id}] -->'
    else:
        return f'\n\n<!-- [embedded {obj_type}: {obj_id}] -->\n\n'


HEADING_COMMANDS = {
    'majorheading': 1,
    'chapter': 1,
    'heading': 2,
    'section': 2,
    'subsection': 3,
    'subsubsection': 4,
    'paragraph': 4,
}

INLINE_COMMANDS = {
    'bold': '**',
    'italic': '*',
    'typewriter': '`',
}

PASS_THROUGH_COMMANDS = {
    'bigger', 'smaller',
    'indent', 'leftindent', 'rightindent',
    'description', 'example', 'display',
    'literal', 'quotation',
    'global', 'note',
}

BLOCK_COMMANDS = {
    'center': None,
    'flushleft': None,
    'flushright': None,
}


def convert_formatting(text, state):
    result = []
    i = 0
    while i < len(text):
        if text[i] == '\\' and i + 1 < len(text) and text[i + 1].isalpha():
            cmd, arg, end = parse_command(text, i)
            if cmd is None:
                result.append(text[i])
                i += 1
                continue

            if cmd in HEADING_COMMANDS:
                level = HEADING_COMMANDS[cmd]
                cleaned_arg = re.sub(r'\\\s*\n', '\n', arg)
                inner = convert_formatting(cleaned_arg.strip(), state)
                inner = strip_all_markdown_formatting(inner).strip()
                inner = inner.replace('\n', ' ')
                inner = re.sub(r'\s+', ' ', inner)
                result.append(f'\n\n{"#" * level} {inner}\n\n')
                i = end
            elif cmd in INLINE_COMMANDS:
                marker = INLINE_COMMANDS[cmd]
                already_active = marker in state.active_markers
                if not already_active:
                    state.active_markers.add(marker)
                inner = convert_formatting(arg, state)
                if not already_active:
                    state.active_markers.discard(marker)
                inner_stripped = inner.strip()
                if inner_stripped:
                    if already_active:
                        result.append(inner)
                    else:
                        inner_stripped = strip_redundant_markers(inner_stripped, marker)
                        leading = inner[:len(inner) - len(inner.lstrip())]
                        trailing = inner[len(inner.rstrip()):]
                        result.append(leading + marker + inner_stripped + marker + trailing)
                i = end
            elif cmd in PASS_THROUGH_COMMANDS:
                inner = convert_formatting(arg, state)
                result.append(inner)
                i = end
            elif cmd == 'center':
                inner = convert_formatting(arg, state).strip()
                result.append(f'\n\n{inner}\n\n')
                i = end
            elif cmd in ('flushleft', 'flushright'):
                inner = convert_formatting(arg, state).strip()
                result.append(f'\n\n{inner}\n\n')
                i = end
            elif cmd == 'formatnote':
                i = end
            elif cmd == 'footnote':
                inner = convert_formatting(arg, state)
                i = end
            elif cmd == 'smaller' or cmd == 'bigger':
                inner = convert_formatting(arg, state)
                result.append(inner)
                i = end
            else:
                inner = convert_formatting(arg, state)
                result.append(inner)
                i = end
        else:
            result.append(text[i])
            i += 1
    return ''.join(result)


def parse_command(text, pos):
    if text[pos] != '\\':
        return None, None, pos

    m = re.match(r'\\(\w+)\{', text[pos:])
    if not m:
        return None, None, pos

    cmd = m.group(1)
    if cmd in ('begindata', 'enddata', 'view', 'textdsversion', 'template', 'define'):
        return None, None, pos

    brace_start = pos + m.end() - 1
    depth = 0
    j = brace_start
    while j < len(text):
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                arg = text[brace_start + 1:j]
                return cmd, arg, j + 1
        j += 1
    return cmd, text[brace_start + 1:], len(text)


def strip_redundant_markers(text, marker):
    if marker == '`':
        while text.startswith('`') and text.endswith('`') and len(text) > 1:
            text = text[1:-1]
    elif marker == '**':
        while text.startswith('**') and text.endswith('**') and len(text) > 3:
            text = text[2:-2].strip()
    elif marker == '*':
        while (text.startswith('*') and text.endswith('*')
               and not text.startswith('**') and len(text) > 1):
            text = text[1:-1].strip()
    return text


def strip_all_markdown_formatting(text):
    text = re.sub(r'\*\*([^*]+)\*\*', r'\1', text)
    text = re.sub(r'\*([^*]+)\*', r'\1', text)
    text = re.sub(r'`([^`]+)`', r'\1', text)
    return text


def clean_continuation_backslashes(text):
    text = re.sub(r'\\\s*\n', '\n', text)
    text = re.sub(r'\\\s*$', '', text)
    return text


def collapse_blank_lines(text):
    text = re.sub(r'\n{4,}', '\n\n\n', text)
    text = text.strip() + '\n'
    return text


def main():
    parser = argparse.ArgumentParser(description='Convert ATK/ez documents to Markdown')
    parser.add_argument('input', nargs='?', help='Input .ez file (default: stdin)')
    parser.add_argument('-o', '--output', help='Output .md file (default: stdout)')
    args = parser.parse_args()

    if args.input:
        with open(args.input, 'r', encoding='utf-8', errors='replace') as f:
            text = f.read()
    else:
        text = sys.stdin.read()

    md = parse_ez(text)

    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(md)
    else:
        sys.stdout.write(md)


if __name__ == '__main__':
    main()
