#include "kj/io.h"
#include "test.capnp.h"
#include "test.pb.h"
#include <benchmark/benchmark.h>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <lz4.h>

#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

constexpr int fromPayloadSize       = 0;
constexpr int toPayloadSize         = 1024;
constexpr int payloadSizeMultiplier = 2;

std::random_device rd;
std::mt19937 rng(rd());
std::uniform_int_distribution< unsigned int > random_integer(1, 1024);
std::uniform_real_distribution< float > random_float(1.0f, 100.0f);
std::uniform_int_distribution< unsigned char > random_byte(std::numeric_limits< unsigned char >::min(),
														   std::numeric_limits< unsigned char >::max());

struct Data {
	std::uint32_t senderSession;
	std::uint64_t frameNumber;
	std::uint32_t targetOrContext;
	std::array< float, 3 > position;
	float volumeAdjustment;
	unsigned char *payload;
	std::size_t payloadSize;
};

Data data;
unsigned char *dataBlob = nullptr;
ProtobufTest::AudioData protobufAudioData;
ProtobufTest::AudioPart protobufAudioPart;

std::vector< unsigned char > encodedData;

Data generateData(const benchmark::State &state) {
	Data data;
	data.senderSession    = random_integer(rng);
	data.frameNumber      = random_integer(rng);
	data.targetOrContext  = random_integer(rng);
	data.position         = { random_float(rng), random_float(rng), random_float(rng) };
	data.volumeAdjustment = random_float(rng);
	data.payload          = dataBlob;
	data.payloadSize      = state.range(0);

	return data;
}

static void setup_data(const benchmark::State &state) {
	rng.seed(42);
	random_integer.reset();
	random_float.reset();
	random_byte.reset();

	dataBlob = new unsigned char[state.range(0)];

	// Educated guess at "large enough"
	encodedData.reserve(state.range(0) * 10 + 1000);

	// Populate dataBlob
	for (std::size_t i = 0; i < state.range(0); ++i) {
		dataBlob[i] = random_byte(rng);
	}

	data = generateData(state);
}

static void teardown_data(const benchmark::State &state) {
	delete[] dataBlob;

	dataBlob = nullptr;
}

static void setup_protobuf(const benchmark::State &state) {
	setup_data(state);

	// Write all fields once to ensure memory is allocated
	protobufAudioData.set_sender_session(42);
	protobufAudioData.set_frame_number(42);
	protobufAudioData.set_opus_data(dataBlob, state.range(0));
	protobufAudioData.set_is_terminator(true);
	protobufAudioData.set_targetorcontext(42);
	protobufAudioData.set_volume_adjustment(1.25f);

	for (int i = 0; i < 3; ++i) {
		protobufAudioData.add_positional_data(3.14159);
		protobufAudioPart.add_positional_data(3.14159);
	}

	protobufAudioPart.set_targetorcontext(42);
	protobufAudioPart.set_volume_adjustment(1.25f);

	// But clear the data out again
	protobufAudioData.Clear();
	protobufAudioPart.Clear();
}

static void teardown_protobuf(const benchmark::State &state) {
	teardown_data(state);
}

static void BM_protobufEncodeFull(benchmark::State &state) {
	// Init counter
	state.counters["encodedSize"] = 0;

	for (auto _ : state) {
		protobufAudioData.Clear();

		// Write data into message and serialize it
		protobufAudioData.set_sender_session(data.senderSession);
		protobufAudioData.set_frame_number(data.frameNumber);
		protobufAudioData.set_opus_data(data.payload, data.payloadSize);
		protobufAudioData.set_is_terminator(true);
		protobufAudioData.set_targetorcontext(data.targetOrContext);
		for (int i = 0; i < 3; ++i) {
			protobufAudioData.add_positional_data(data.position[i]);
		}
		protobufAudioData.set_volume_adjustment(data.volumeAdjustment);

		std::size_t encodedSize = protobufAudioData.ByteSizeLong();

		encodedData.resize(encodedSize);

		if (!protobufAudioData.SerializeToArray(encodedData.data(), encodedData.size())) {
			state.SkipWithError("Failed to encode Protobuf message");
		} else {
			// Write size to variable
			state.counters["encodedSize"] = encodedSize;
		}
	}
}

BENCHMARK(BM_protobufEncodeFull)
	->RangeMultiplier(payloadSizeMultiplier)
	->Range(fromPayloadSize, toPayloadSize)
	->Setup(setup_protobuf)
	->Teardown(teardown_protobuf);

static void BM_protobufEncodePartial(benchmark::State &state) {
	// Init counter
	state.counters["encodedSize"] = 0;

	// Write audioData outside to emulate encoding it once and then sticking with it
	protobufAudioData.Clear();

	protobufAudioData.set_sender_session(data.senderSession);
	protobufAudioData.set_frame_number(data.frameNumber);
	protobufAudioData.set_opus_data(data.payload, data.payloadSize);
	protobufAudioData.set_is_terminator(true);

	std::size_t fixedPartSize = protobufAudioData.ByteSizeLong();

	encodedData.resize(fixedPartSize);

	if (!protobufAudioData.SerializeToArray(encodedData.data(), fixedPartSize)) {
		state.SkipWithError("Failed to encode fixed part");
	}

	for (auto _ : state) {
		protobufAudioPart.Clear();

		// Write data into part, serialize it and append it to the already-seralized fixed part
		protobufAudioPart.set_targetorcontext(data.targetOrContext);
		for (int i = 0; i < 3; ++i) {
			protobufAudioPart.add_positional_data(data.position[i]);
		}
		protobufAudioPart.set_volume_adjustment(data.volumeAdjustment);

		std::size_t encodedSize = protobufAudioPart.ByteSizeLong();

		encodedData.resize(encodedSize + fixedPartSize);

		if (!protobufAudioPart.SerializeToArray(encodedData.data() + fixedPartSize, encodedData.size())) {
			state.SkipWithError("Failed to encode Protobuf message");
		} else {
			// Write size to variable
			state.counters["encodedSize"] = encodedSize + fixedPartSize;
		}
	}
}

BENCHMARK(BM_protobufEncodePartial)
	->RangeMultiplier(payloadSizeMultiplier)
	->Range(fromPayloadSize, toPayloadSize)
	->Setup(setup_protobuf)
	->Teardown(teardown_protobuf);

static void BM_capnProtoEcodeOnly(benchmark::State &state) {
	// Init counter
	state.counters["encodedSize"] = 0;

	encodedData.resize(encodedData.capacity());

	::capnp::word scratch[200] = {};

	for (auto _ : state) {
		::capnp::MallocMessageBuilder builder(::kj::arrayPtr(scratch, sizeof(scratch)),
											  ::capnp::AllocationStrategy::FIXED_SIZE);

		CapnProtoTest::AudioData::Builder messageBuilder = builder.initRoot< CapnProtoTest::AudioData >();

		messageBuilder.setSenderSession(data.senderSession);
		messageBuilder.setFrameNumber(data.frameNumber);
		messageBuilder.setOpusData(kj::arrayPtr(data.payload, data.payloadSize));
		messageBuilder.setIsTerminator(true);
		messageBuilder.setTargetOrContext(data.targetOrContext);

		auto positionBuilder = messageBuilder.initPositionalData(3);
		for (int i = 0; i < 3; ++i) {
			positionBuilder.set(i, data.position[i]);
		}

		messageBuilder.setVolumeAdjustment(data.volumeAdjustment);

		std::size_t encodedSize = builder.sizeInWords() * 8;

		// Write size to variable
		state.counters["encodedSize"] = encodedSize;
	}
}

BENCHMARK(BM_capnProtoEcodeOnly)
	->RangeMultiplier(payloadSizeMultiplier)
	->Range(fromPayloadSize, toPayloadSize)
	->Setup(setup_data)
	->Teardown(teardown_data);

static void BM_capnProtoEcodeAndPack(benchmark::State &state) {
	// Init counter
	state.counters["encodedSize"] = 0;

	encodedData.resize(encodedData.capacity());

	::capnp::word scratch[200] = {};

	for (auto _ : state) {
		::kj::ArrayOutputStream outStream(::kj::arrayPtr(encodedData.data(), encodedData.size()));

		::capnp::MallocMessageBuilder builder(::kj::arrayPtr(scratch, sizeof(scratch)),
											  ::capnp::AllocationStrategy::FIXED_SIZE);

		CapnProtoTest::AudioData::Builder messageBuilder = builder.initRoot< CapnProtoTest::AudioData >();

		messageBuilder.setSenderSession(data.senderSession);
		messageBuilder.setFrameNumber(data.frameNumber);
		messageBuilder.setOpusData(kj::arrayPtr(data.payload, data.payloadSize));
		messageBuilder.setIsTerminator(true);
		messageBuilder.setTargetOrContext(data.targetOrContext);

		auto positionBuilder = messageBuilder.initPositionalData(3);
		for (int i = 0; i < 3; ++i) {
			positionBuilder.set(i, data.position[i]);
		}

		messageBuilder.setVolumeAdjustment(data.volumeAdjustment);

		::capnp::writePackedMessage(outStream, builder);
		std::size_t encodedSize = outStream.getArray().size();

		// Write size to variable
		state.counters["encodedSize"] = encodedSize;
	}
}

BENCHMARK(BM_capnProtoEcodeAndPack)
	->RangeMultiplier(payloadSizeMultiplier)
	->Range(fromPayloadSize, toPayloadSize)
	->Setup(setup_data)
	->Teardown(teardown_data);


static void BM_capnProtoEcodeAndCompress(benchmark::State &state) {
	// Init counter
	state.counters["encodedSize"] = 0;

	encodedData.resize(encodedData.capacity());

	::capnp::word scratch[200] = {};

	for (auto _ : state) {
		::capnp::MallocMessageBuilder builder(::kj::arrayPtr(scratch, sizeof(scratch)),
											  ::capnp::AllocationStrategy::FIXED_SIZE);

		CapnProtoTest::AudioData::Builder messageBuilder = builder.initRoot< CapnProtoTest::AudioData >();

		messageBuilder.setSenderSession(data.senderSession);
		messageBuilder.setFrameNumber(data.frameNumber);
		messageBuilder.setOpusData(kj::arrayPtr(data.payload, data.payloadSize));
		messageBuilder.setIsTerminator(true);
		messageBuilder.setTargetOrContext(data.targetOrContext);

		auto positionBuilder = messageBuilder.initPositionalData(3);
		for (int i = 0; i < 3; ++i) {
			positionBuilder.set(i, data.position[i]);
		}

		messageBuilder.setVolumeAdjustment(data.volumeAdjustment);

		std::size_t encodedSize = LZ4_compress_default(
			reinterpret_cast< const char * >(scratch), reinterpret_cast< char * >(encodedData.data()),
			builder.sizeInWords() * (sizeof(::capnp::word) / sizeof(char)), encodedData.size());

		// Write size to variable
		state.counters["encodedSize"] = encodedSize;
	}
}

BENCHMARK(BM_capnProtoEcodeAndCompress)
	->RangeMultiplier(payloadSizeMultiplier)
	->Range(fromPayloadSize, toPayloadSize)
	->Setup(setup_data)
	->Teardown(teardown_data);

BENCHMARK_MAIN();
