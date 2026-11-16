/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KUI_TOY_TEST_BUILD_H
#define KUI_TOY_TEST_BUILD_H
/* KUI_TOY_TEST_BUILD is 1 when a Toy Commander test module, or one of its
 * host tests, is built (the switches below) and 0 in every production build.
 *
 * The test modules restructured parts of the shared reader sources. Where they
 * did, the source keeps both texts: the test modules' under
 * `#if KUI_TOY_TEST_BUILD`, and under its `#else` the production text exactly
 * as it was before the test sources were imported. So a production build
 * compiles the reviewed text byte for byte, and each delivered test module
 * still builds from the text it was built from. tests/test_toy_test_switches.py
 * holds both to that. */
#if defined(KUI_TOY_SUITE_MODE) || KUI_DISC_TOY_TRACE || KUI_TOY_WIRE_PROBE || \
    KUI_TOY_WIRE_STREAM || KUI_CD_EDC_CACHED_PAIR || KUI_NATIVE_TOY_CACHE_CAPTURE || \
    KUI_NATIVE_TOY_HEADER_REPLAY || KUI_NATIVE_TOY_MOVIE_CAPTURE || \
    KUI_NATIVE_TOY_PLAYBACK_CAPTURE || defined(KUI_WIRE_STREAM_TEST) || \
    defined(KUI_TOY_BURST_PRODUCER_TEST) || defined(KUI_TOY_WITNESS_HOST_TEST) || \
    defined(KUI_TOY_SCI_BURST_HOST_TEST)
#define KUI_TOY_TEST_BUILD 1
#else
#define KUI_TOY_TEST_BUILD 0
#endif
#endif
