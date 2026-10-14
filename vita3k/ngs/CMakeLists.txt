add_library(
	ngs
	STATIC
	src/modules/atrac9.cpp
	src/modules/compressor.cpp
	src/modules/delay.cpp
	src/modules/distortion.cpp
	src/modules/envelope.cpp
	src/modules/equalizer.cpp
	src/modules/filter.cpp
	src/modules/generator.cpp
	src/modules/mixer.cpp
	src/modules/output.cpp
	src/modules/pauser.cpp
	src/modules/pitchshift.cpp
	src/modules/player.cpp
	src/modules/reverb.cpp
	src/definitions.cpp
	src/dsp.cpp
	src/ngs.cpp
	src/param_log.cpp
	src/rate_resampler.cpp
	src/route.cpp
	src/scheduler.cpp)

target_include_directories(ngs PUBLIC include)
target_link_libraries(ngs PUBLIC codec)
target_link_libraries(ngs PRIVATE util mem kernel cpu ffmpeg)

if(NOT ANDROID)
	# dsp.cpp has no emulator dependencies, so the tests build it directly.
	add_executable(
		ngs-tests
		src/dsp.cpp
		tests/dsp_tests.cpp
	)

	target_include_directories(ngs-tests PRIVATE include)
	target_link_libraries(ngs-tests PRIVATE googletest)
	add_test(NAME ngs COMMAND ngs-tests)

	# The scheduler tests drive Voice and VoiceScheduler, which need guest
	# memory and a KernelState, so they link the whole ngs library.
	add_executable(
		ngs-scheduler-tests
		tests/scheduler_tests.cpp
	)

	# kernel/state.h reaches cpu/common.h and rtc/rtc.h, which reach fmt and
	# spdlog, and the kernel target keeps those include directories to itself.
	target_include_directories(ngs-scheduler-tests PRIVATE
		include
		../mem/include
		../kernel/include
		../cpu/include
		../rtc/include
		../emuenv/include
		../util/include
		../../external/fmt/include
		../../external/spdlog/include)
	target_link_libraries(ngs-scheduler-tests PRIVATE ngs googletest)
	add_test(NAME ngs-scheduler COMMAND ngs-scheduler-tests)
endif()
