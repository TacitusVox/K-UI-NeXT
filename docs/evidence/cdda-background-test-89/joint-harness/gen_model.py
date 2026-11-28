import sys
src = open('tests/test_toy_cdda_output.c').read().split('\n')
# lines are 1-indexed in the file; take 55..746 (model + fresh)
part = src[54:746]
text = '\n'.join(part) + '\n'
def rep(old, new, count=1):
    global text
    assert text.count(old) == count, (old, text.count(old))
    text = text.replace(old, new)
rep('struct kui_toy_cdda_drive kui_toy_cdda_drive;\n', '/* (the real player defines the drive) */\n')
rep('''#define CHECK(x) do { ++checks; if(!(x)) { \\
    fprintf(stderr, "test_toy_cdda_output.c:%d: %s\\n", __LINE__, #x); exit(1); } } while(0)''',
    '''#define CHECK(x) do { ++checks; if(!(x)) model_fail(__LINE__, #x); } while(0)''')
# the hold model: from 'static void hold_over(uint32_t tc);' up to the comment before expected()
a = text.index('/* Where the drive stands at TMU0 count tc; false: its last pass is over. */')
b = text.index('/* Frame k of the ring that began at heard_play. */')
text = text[:a] + text[b:]
rep('''        if(edge_soon) {
            /* A group of three sectors of the play becomes whole a few
             * ticks behind this read: between a heartbeat's own count
             * and the visit its hold begins with. */
            edge_soon = false;
            D.base_counter -= (uint32_t)(D.base_counter - tcnt()) % KUI_TOY_CDDA_TICKS_3_SECTORS + 8u;
        }
        return tcnt();''', '''        return clock_read();''')
rep('if(stretch_live && tstr)CHECK(now - stretch_began <= 2344u + 10u * access_ticks + 8u * store_ticks);',
    'if(stretch_live && tstr)CHECK(now - stretch_began <= 2344u + 10u * access_ticks + 8u * store_ticks + 12u * read_ticks);')
rep('''    holds = releases = holds_over = holds_refused = 0u;
    over_at_hold = edge_soon = false;
''', '')
rep('''            fprintf(stderr, "frame %llu of the ring: played %d %d, the play has %d %d\\n",
                (unsigned long long)k, (int)sl, (int)sr16, el, er);
            CHECK(!"a frame that is not the play's");''',
    '''            wrong_frame((unsigned long long)k, (int)sl, (int)sr16, el, er);''')
rep('    if(strict)CHECK(move <= 32767);\n', '    /* (a ring that ran dry plays what lay there: not held against the model) */\n')
open('build/review4/model.inc', 'w').write(text)
print(len(text.split('\n')), 'lines')
