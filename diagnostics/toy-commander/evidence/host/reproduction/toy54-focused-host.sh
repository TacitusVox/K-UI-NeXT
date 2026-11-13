mkdir -p build/host54-focused
set -e; for mode in 54; do \
 cc -std=c11 -O1 -g -Wall -Wextra -Werror -Wpedantic -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -Iinclude -Isrc/loader -O2 -DKUI_NATIVE_READER_DIAGNOSTIC=1 -DKUI_RETAIL_NATIVE_HYBRID=1 -DKUI_SCI_SD_NO_BLOCK=1 -DKUI_GENERAL_SCI_RESUME_FIX=1 -DKUI_RETAIL_NATIVE_DISC_CHECK=1 -DKUI_RETAIL_NATIVE_DRIVE_STATE=1 -DKUI_RETAIL_ASYNC_TEST=1 -DKUI_SCI_STREAM_TEST=1 -DKUI_RETAIL_ASYNC=1 -DKUI_RETAIL_GD_ASYNC=1 -DKUI_RETAIL_TRANSPORT=1 -Itests \
  -DKUI_NATIVE_REQUEST_TIMING=1 -DKUI_NATIVE_COOP_PUMP=1 -DKUI_NATIVE_IRQ_REARM=1 \
  -DKUI_NATIVE_LOADER_TIMELINE=1 -DKUI_VMU_NATIVE_BACKGROUND=1 -DKUI_NATIVE_SDK_SCI_ROUTE=1 \
  -DKUI_NATIVE_SDK_SCI_STRUCTURAL=1 -DKUI_NATIVE_READAHEAD=1 -DKUI_NATIVE_READAHEAD_AUX_HOST_TEST=1 \
  -DKUI_NATIVE_TOY_MOVIE_CAPTURE=1 -DKUI_NATIVE_TOY_PLAYBACK_CAPTURE=1 -DKUI_TOY_SUITE_MODE=$mode -DKUI_CD_EDC_CACHED_PAIR=1 \
  src/loader/retail_async.c src/loader/sci_stream.c src/core/retail_cursor.c src/core/retail_image.c src/core/retail_gd.c src/loader/native_readahead_aux.c src/core/native_readahead_cache.c \
  src/core/hash.c src/core/data.c tests/test_toy_resume_integration.c \
  -Wl,--gc-sections -o build/host54-focused/test-toy-sdk-liveness-$mode; \
 ASAN_OPTIONS=detect_leaks=0 build/host54-focused/test-toy-sdk-liveness-$mode; \
done
